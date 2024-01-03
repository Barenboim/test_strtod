#include <errno.h>
#include <ctype.h>

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#ifndef NULL
#define NULL 0
#endif

static int maxExponent = 511;	/* Largest possible base 10 exponent.  Any
				 * exponent larger than this will already
				 * produce underflow or overflow, so there's
				 * no need to worry about additional digits.
				 */
static double powersOf10[] = {	/* Table giving binary powers of 10.  Entry */
    10.,			/* is 10^2^i.  Used to convert decimal */
    100.,			/* exponents into floating-point numbers. */
    1.0e4,
    1.0e8,
    1.0e16,
    1.0e32,
    1.0e64,
    1.0e128,
    1.0e256
};

/*
 *----------------------------------------------------------------------
 *
 * strtod --
 *
 *	This procedure converts a floating-point number from an ASCII
 *	decimal representation to internal double-precision format.
 *
 * Results:
 *	The return value is the double-precision floating-point
 *	representation of the characters in string.  If endPtr isn't
 *	NULL, then *endPtr is filled in with the address of the
 *	next character after the last one that was part of the
 *	floating-point number.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

double
apple_strtod(const char *string, char **endPtr)
    	/* A decimal ASCII floating-point number,
				 * optionally preceded by white space.
				 * Must have form "-I.FE-X", where I is the
				 * integer part of the mantissa, F is the
				 * fractional part of the mantissa, and X
				 * is the exponent.  Either of the signs
				 * may be "+", "-", or omitted.  Either I
				 * or F may be omitted, or both.  The decimal
				 * point isn't necessary unless F is present.
				 * The "E" may actually be an "e".  E and X
				 * may both be omitted (but not just one).
				 */
   		/* If non-NULL, store terminating character's
				 * address here. */
{
    int sign, expSign = FALSE;
    double fraction, dblExp, *d;
    const char *p;
    int c;
    int exp = 0;		/* Exponent read from "EX" field. */
    int fracExp = 0;		/* Exponent that derives from the fractional
				 * part.  Under normal circumstatnces, it is
				 * the negative of the number of digits in F.
				 * However, if I is very long, the last digits
				 * of I get dropped (otherwise a long I with a
				 * large negative exponent could cause an
				 * unnecessary overflow on I alone).  In this
				 * case, fracExp is incremented one for each
				 * dropped digit. */
    int mantSize;		/* Number of digits in mantissa. */
    int decPt;			/* Number of mantissa digits BEFORE decimal
				 * point. */
    const char *pExp;		/* Temporarily holds location of exponent
				 * in string. */

    /*
     * Strip off leading blanks and check for a sign.
     */

    p = string;
    while (isspace((unsigned char)(*p))) {
	p += 1;
    }
    if (*p == '-') {
	sign = TRUE;
	p += 1;
    } else {
	if (*p == '+') {
	    p += 1;
	}
	sign = FALSE;
    }

    /*
     * Count the number of digits in the mantissa (including the decimal
     * point), and also locate the decimal point.
     */

    decPt = -1;
    for (mantSize = 0; ; mantSize += 1)
    {
	c = *p;
	if (!isdigit(c)) {
	    if ((c != '.') || (decPt >= 0)) {
		break;
	    }
	    decPt = mantSize;
	}
	p += 1;
    }

    /*
     * Now suck up the digits in the mantissa.  Use two integers to
     * collect 9 digits each (this is faster than using floating-point).
     * If the mantissa has more than 18 digits, ignore the extras, since
     * they can't affect the value anyway.
     */
    
    pExp  = p;
    p -= mantSize;
    if (decPt < 0) {
	decPt = mantSize;
    } else {
	mantSize -= 1;			/* One of the digits was the point. */
    }
    if (mantSize > 18) {
	fracExp = decPt - 18;
	mantSize = 18;
    } else {
	fracExp = decPt - mantSize;
    }
    if (mantSize == 0) {
	fraction = 0.0;
	p = string;
	goto done;
    } else {
	int frac1, frac2;
	frac1 = 0;
	for ( ; mantSize > 9; mantSize -= 1)
	{
	    c = *p;
	    p += 1;
	    if (c == '.') {
		c = *p;
		p += 1;
	    }
	    frac1 = 10*frac1 + (c - '0');
	}
	frac2 = 0;
	for (; mantSize > 0; mantSize -= 1)
	{
	    c = *p;
	    p += 1;
	    if (c == '.') {
		c = *p;
		p += 1;
	    }
	    frac2 = 10*frac2 + (c - '0');
	}
	fraction = (1.0e9 * frac1) + frac2;
    }

    /*
     * Skim off the exponent.
     */

    p = pExp;
    if ((*p == 'E') || (*p == 'e')) {
	p += 1;
	if (*p == '-') {
	    expSign = TRUE;
	    p += 1;
	} else {
	    if (*p == '+') {
		p += 1;
	    }
	    expSign = FALSE;
	}
	if (!isdigit((unsigned char)(*p))) {
	    p = pExp;
	    goto done;
	}
	while (isdigit((unsigned char)(*p))) {
	    exp = exp * 10 + (*p - '0');
	    p += 1;
	}
    }
    if (expSign) {
	exp = fracExp - exp;
    } else {
	exp = fracExp + exp;
    }

    /*
     * Generate a floating-point number that represents the exponent.
     * Do this by processing the exponent one bit at a time to combine
     * many powers of 2 of 10. Then combine the exponent with the
     * fraction.
     */
    
    if (exp < 0) {
	expSign = TRUE;
	exp = -exp;
    } else {
	expSign = FALSE;
    }
    if (exp > maxExponent) {
	exp = maxExponent;
	errno = ERANGE;
    }
    dblExp = 1.0;
    for (d = powersOf10; exp != 0; exp >>= 1, d += 1) {
	if (exp & 01) {
	    dblExp *= *d;
	}
    }
    if (expSign) {
	fraction /= dblExp;
    } else {
	fraction *= dblExp;
    }

done:
    if (endPtr != NULL) {
	*endPtr = (char *) p;
    }

    if (sign) {
	return -fraction;
    }
    return fraction;
}

#include <math.h>

static const double __power_of_10[309] = {
	1.0e0,   1.0e1,   1.0e2,   1.0e3,   1.0e4,
	1.0e5,   1.0e6,   1.0e7,   1.0e8,   1.0e9,
	1.0e10,  1.0e11,  1.0e12,  1.0e13,  1.0e14,
	1.0e15,  1.0e16,  1.0e17,  1.0e18,  1.0e19,
	1.0e20,  1.0e21,  1.0e22,  1.0e23,  1.0e24,
	1.0e25,  1.0e26,  1.0e27,  1.0e28,  1.0e29,
	1.0e30,  1.0e31,  1.0e32,  1.0e33,  1.0e34,
	1.0e35,  1.0e36,  1.0e37,  1.0e38,  1.0e39,
	1.0e40,  1.0e41,  1.0e42,  1.0e43,  1.0e44,
	1.0e45,  1.0e46,  1.0e47,  1.0e48,  1.0e49,
	1.0e50,  1.0e51,  1.0e52,  1.0e53,  1.0e54,
	1.0e55,  1.0e56,  1.0e57,  1.0e58,  1.0e59,
	1.0e60,  1.0e61,  1.0e62,  1.0e63,  1.0e64,
	1.0e65,  1.0e66,  1.0e67,  1.0e68,  1.0e69,
	1.0e70,  1.0e71,  1.0e72,  1.0e73,  1.0e74,
	1.0e75,  1.0e76,  1.0e77,  1.0e78,  1.0e79,
	1.0e80,  1.0e81,  1.0e82,  1.0e83,  1.0e84,
	1.0e85,  1.0e86,  1.0e87,  1.0e88,  1.0e89,
	1.0e90,  1.0e91,  1.0e92,  1.0e93,  1.0e94,
	1.0e95,  1.0e96,  1.0e97,  1.0e98,  1.0e99,
	1.0e100, 1.0e101, 1.0e102, 1.0e103, 1.0e104,
	1.0e105, 1.0e106, 1.0e107, 1.0e108, 1.0e109,
	1.0e110, 1.0e111, 1.0e112, 1.0e113, 1.0e114,
	1.0e115, 1.0e116, 1.0e117, 1.0e118, 1.0e119,
	1.0e120, 1.0e121, 1.0e122, 1.0e123, 1.0e124,
	1.0e125, 1.0e126, 1.0e127, 1.0e128, 1.0e129,
	1.0e130, 1.0e131, 1.0e132, 1.0e133, 1.0e134,
	1.0e135, 1.0e136, 1.0e137, 1.0e138, 1.0e139,
	1.0e140, 1.0e141, 1.0e142, 1.0e143, 1.0e144,
	1.0e145, 1.0e146, 1.0e147, 1.0e148, 1.0e149,
	1.0e150, 1.0e151, 1.0e152, 1.0e153, 1.0e154,
	1.0e155, 1.0e156, 1.0e157, 1.0e158, 1.0e159,
	1.0e160, 1.0e161, 1.0e162, 1.0e163, 1.0e164,
	1.0e165, 1.0e166, 1.0e167, 1.0e168, 1.0e169,
	1.0e170, 1.0e171, 1.0e172, 1.0e173, 1.0e174,
	1.0e175, 1.0e176, 1.0e177, 1.0e178, 1.0e179,
	1.0e180, 1.0e181, 1.0e182, 1.0e183, 1.0e184,
	1.0e185, 1.0e186, 1.0e187, 1.0e188, 1.0e189,
	1.0e190, 1.0e191, 1.0e192, 1.0e193, 1.0e194,
	1.0e195, 1.0e196, 1.0e197, 1.0e198, 1.0e199,
	1.0e200, 1.0e201, 1.0e202, 1.0e203, 1.0e204,
	1.0e205, 1.0e206, 1.0e207, 1.0e208, 1.0e209,
	1.0e210, 1.0e211, 1.0e212, 1.0e213, 1.0e214,
	1.0e215, 1.0e216, 1.0e217, 1.0e218, 1.0e219,
	1.0e220, 1.0e221, 1.0e222, 1.0e223, 1.0e224,
	1.0e225, 1.0e226, 1.0e227, 1.0e228, 1.0e229,
	1.0e230, 1.0e231, 1.0e232, 1.0e233, 1.0e234,
	1.0e235, 1.0e236, 1.0e237, 1.0e238, 1.0e239,
	1.0e240, 1.0e241, 1.0e242, 1.0e243, 1.0e244,
	1.0e245, 1.0e246, 1.0e247, 1.0e248, 1.0e249,
	1.0e250, 1.0e251, 1.0e252, 1.0e253, 1.0e254,
	1.0e255, 1.0e256, 1.0e257, 1.0e258, 1.0e259,
	1.0e260, 1.0e261, 1.0e262, 1.0e263, 1.0e264,
	1.0e265, 1.0e266, 1.0e267, 1.0e268, 1.0e269,
	1.0e270, 1.0e271, 1.0e272, 1.0e273, 1.0e274,
	1.0e275, 1.0e276, 1.0e277, 1.0e278, 1.0e279,
	1.0e280, 1.0e281, 1.0e282, 1.0e283, 1.0e284,
	1.0e285, 1.0e286, 1.0e287, 1.0e288, 1.0e289,
	1.0e290, 1.0e291, 1.0e292, 1.0e293, 1.0e294,
	1.0e295, 1.0e296, 1.0e297, 1.0e298, 1.0e299,
	1.0e300, 1.0e301, 1.0e302, 1.0e303, 1.0e304,
	1.0e305, 1.0e306, 1.0e307, 1.0e308,
};

static const long double __lpower_of_10[342] = {
	1.0e0l,   1.0e1l,   1.0e2l,   1.0e3l,   1.0e4l,
	1.0e5l,   1.0e6l,   1.0e7l,   1.0e8l,   1.0e9l,
	1.0e10l,  1.0e11l,  1.0e12l,  1.0e13l,  1.0e14l,
	1.0e15l,  1.0e16l,  1.0e17l,  1.0e18l,  1.0e19l,
	1.0e20l,  1.0e21l,  1.0e22l,  1.0e23l,  1.0e24l,
	1.0e25l,  1.0e26l,  1.0e27l,  1.0e28l,  1.0e29l,
	1.0e30l,  1.0e31l,  1.0e32l,  1.0e33l,  1.0e34l,
	1.0e35l,  1.0e36l,  1.0e37l,  1.0e38l,  1.0e39l,
	1.0e40l,  1.0e41l,  1.0e42l,  1.0e43l,  1.0e44l,
	1.0e45l,  1.0e46l,  1.0e47l,  1.0e48l,  1.0e49l,
	1.0e50l,  1.0e51l,  1.0e52l,  1.0e53l,  1.0e54l,
	1.0e55l,  1.0e56l,  1.0e57l,  1.0e58l,  1.0e59l,
	1.0e60l,  1.0e61l,  1.0e62l,  1.0e63l,  1.0e64l,
	1.0e65l,  1.0e66l,  1.0e67l,  1.0e68l,  1.0e69l,
	1.0e70l,  1.0e71l,  1.0e72l,  1.0e73l,  1.0e74l,
	1.0e75l,  1.0e76l,  1.0e77l,  1.0e78l,  1.0e79l,
	1.0e80l,  1.0e81l,  1.0e82l,  1.0e83l,  1.0e84l,
	1.0e85l,  1.0e86l,  1.0e87l,  1.0e88l,  1.0e89l,
	1.0e90l,  1.0e91l,  1.0e92l,  1.0e93l,  1.0e94l,
	1.0e95l,  1.0e96l,  1.0e97l,  1.0e98l,  1.0e99l,
	1.0e100l, 1.0e101l, 1.0e102l, 1.0e103l, 1.0e104l,
	1.0e105l, 1.0e106l, 1.0e107l, 1.0e108l, 1.0e109l,
	1.0e110l, 1.0e111l, 1.0e112l, 1.0e113l, 1.0e114l,
	1.0e115l, 1.0e116l, 1.0e117l, 1.0e118l, 1.0e119l,
	1.0e120l, 1.0e121l, 1.0e122l, 1.0e123l, 1.0e124l,
	1.0e125l, 1.0e126l, 1.0e127l, 1.0e128l, 1.0e129l,
	1.0e130l, 1.0e131l, 1.0e132l, 1.0e133l, 1.0e134l,
	1.0e135l, 1.0e136l, 1.0e137l, 1.0e138l, 1.0e139l,
	1.0e140l, 1.0e141l, 1.0e142l, 1.0e143l, 1.0e144l,
	1.0e145l, 1.0e146l, 1.0e147l, 1.0e148l, 1.0e149l,
	1.0e150l, 1.0e151l, 1.0e152l, 1.0e153l, 1.0e154l,
	1.0e155l, 1.0e156l, 1.0e157l, 1.0e158l, 1.0e159l,
	1.0e160l, 1.0e161l, 1.0e162l, 1.0e163l, 1.0e164l,
	1.0e165l, 1.0e166l, 1.0e167l, 1.0e168l, 1.0e169l,
	1.0e170l, 1.0e171l, 1.0e172l, 1.0e173l, 1.0e174l,
	1.0e175l, 1.0e176l, 1.0e177l, 1.0e178l, 1.0e179l,
	1.0e180l, 1.0e181l, 1.0e182l, 1.0e183l, 1.0e184l,
	1.0e185l, 1.0e186l, 1.0e187l, 1.0e188l, 1.0e189l,
	1.0e190l, 1.0e191l, 1.0e192l, 1.0e193l, 1.0e194l,
	1.0e195l, 1.0e196l, 1.0e197l, 1.0e198l, 1.0e199l,
	1.0e200l, 1.0e201l, 1.0e202l, 1.0e203l, 1.0e204l,
	1.0e205l, 1.0e206l, 1.0e207l, 1.0e208l, 1.0e209l,
	1.0e210l, 1.0e211l, 1.0e212l, 1.0e213l, 1.0e214l,
	1.0e215l, 1.0e216l, 1.0e217l, 1.0e218l, 1.0e219l,
	1.0e220l, 1.0e221l, 1.0e222l, 1.0e223l, 1.0e224l,
	1.0e225l, 1.0e226l, 1.0e227l, 1.0e228l, 1.0e229l,
	1.0e230l, 1.0e231l, 1.0e232l, 1.0e233l, 1.0e234l,
	1.0e235l, 1.0e236l, 1.0e237l, 1.0e238l, 1.0e239l,
	1.0e240l, 1.0e241l, 1.0e242l, 1.0e243l, 1.0e244l,
	1.0e245l, 1.0e246l, 1.0e247l, 1.0e248l, 1.0e249l,
	1.0e250l, 1.0e251l, 1.0e252l, 1.0e253l, 1.0e254l,
	1.0e255l, 1.0e256l, 1.0e257l, 1.0e258l, 1.0e259l,
	1.0e260l, 1.0e261l, 1.0e262l, 1.0e263l, 1.0e264l,
	1.0e265l, 1.0e266l, 1.0e267l, 1.0e268l, 1.0e269l,
	1.0e270l, 1.0e271l, 1.0e272l, 1.0e273l, 1.0e274l,
	1.0e275l, 1.0e276l, 1.0e277l, 1.0e278l, 1.0e279l,
	1.0e280l, 1.0e281l, 1.0e282l, 1.0e283l, 1.0e284l,
	1.0e285l, 1.0e286l, 1.0e287l, 1.0e288l, 1.0e289l,
	1.0e290l, 1.0e291l, 1.0e292l, 1.0e293l, 1.0e294l,
	1.0e295l, 1.0e296l, 1.0e297l, 1.0e298l, 1.0e299l,
	1.0e300l, 1.0e301l, 1.0e302l, 1.0e303l, 1.0e304l,
	1.0e305l, 1.0e306l, 1.0e307l, 1.0e308l, 1.0e309l,
	1.0e310l, 1.0e311l, 1.0e312l, 1.0e313l, 1.0e314l,
	1.0e315l, 1.0e316l, 1.0e317l, 1.0e318l, 1.0e319l,
	1.0e320l, 1.0e321l, 1.0e322l, 1.0e323l, 1.0e324l,
	1.0e325l, 1.0e326l, 1.0e327l, 1.0e328l, 1.0e329l,
	1.0e330l, 1.0e331l, 1.0e332l, 1.0e333l, 1.0e334l,
	1.0e335l, 1.0e336l, 1.0e337l, 1.0e338l, 1.0e339l,
	1.0e340l, 1.0e341l
};

static double __evaluate_json_number(const char *integer,
									 const char *fraction,
									 int exp)
{
	long long mant = 0;
	int figures = 0;
	double num;
	int sign;

	sign = (*integer == '-');
	if (sign)
		integer++;

	if (*integer != '0')
	{
		mant = *integer - '0';
		integer++;
		figures++;
		while (isdigit(*integer) && figures < 18)
		{
			mant *= 10;
			mant += *integer - '0';
			integer++;
			figures++;
		}

		while (isdigit(*integer))
		{
			exp++;
			integer++;
		}
	}
	else
	{
		while (*fraction == '0')
		{
			exp--;
			fraction++;
		}
	}

	while (isdigit(*fraction) && figures < 18)
	{
		mant *= 10;
		mant += *fraction - '0';
		exp--;
		fraction++;
		figures++;
	}

	if (exp != 0 && figures != 0)
	{
		while (exp > 0 && figures < 18)
		{
			mant *= 10;
			exp--;
			figures++;
		}

		while (exp < 0 && mant % 10 == 0)
		{
			mant /= 10;
			exp++;
			figures--;
		}
	}

	if (exp == 0 || figures == 0)
		num = mant;
	else if (exp > 291)
		num = INFINITY;
	else if (exp > 0)
		num = mant * __power_of_10[exp];
	else if (exp > -309)
		num = mant / __power_of_10[-exp];
	else if (exp > -324 - figures)
		num = mant / __power_of_10[-exp - 308] / __power_of_10[308];
	else
		num = 0.0;

	return sign ? -num : num;
}

static int __parse_json_number(const char *cursor, const char **end,
							   double *num)
{
	const char *integer = cursor;
	const char *fraction = "";
	int exp = 0;
	int sign;

	if (*cursor == '-')
		cursor++;

	if (!isdigit(*cursor))
		return -2;

	if (*cursor == '0' && isdigit(cursor[1]))
		return -2;

	cursor++;
	while (isdigit(*cursor))
		cursor++;

	if (*cursor == '.')
	{
		cursor++;
		fraction = cursor;
		if (!isdigit(*cursor))
			return -2;

		cursor++;
		while (isdigit(*cursor))
			cursor++;
	}

	if (*cursor == 'E' || *cursor == 'e')
	{
		cursor++;
		sign = (*cursor == '-');
		if (sign || *cursor == '+')
			cursor++;

		if (!isdigit(*cursor))
			return -2;

		exp = *cursor - '0';
		cursor++;
		while (isdigit(*cursor) && exp < 2000000)
		{
			exp *= 10;
			exp += *cursor - '0';
			cursor++;
		}

		while (isdigit(*cursor))
			cursor++;

		if (sign)
			exp = -exp;
	}

	if (cursor - integer > 1000000)
		return -2;

	*num = __evaluate_json_number(integer, fraction, exp);
	*end = cursor;
	return 0;
}

static double __evaluate_json_number2(const char *integer,
									 const char *fraction,
									 int exp)
{
	long long mant = 0;
	int figures = 0;
	double num;
	int sign;

	sign = (*integer == '-');
	if (sign)
		integer++;

	if (*integer != '0')
	{
		mant = *integer - '0';
		integer++;
		figures++;
		while (isdigit(*integer) && figures < 18)
		{
			mant *= 10;
			mant += *integer - '0';
			integer++;
			figures++;
		}

		while (isdigit(*integer))
		{
			exp++;
			integer++;
		}
	}
	else
	{
		while (*fraction == '0')
		{
			exp--;
			fraction++;
		}
	}

	while (isdigit(*fraction) && figures < 18)
	{
		mant *= 10;
		mant += *fraction - '0';
		exp--;
		fraction++;
		figures++;
	}

	if (figures != 0)
	{
		while (exp > 0 && figures < 18)
		{
			mant *= 10;
			exp--;
			figures++;
		}

		while (exp < 0 && mant % 10 == 0)
		{
			mant /= 10;
			exp++;
			figures--;
		}
	}

	if (exp == 0 || figures == 0)
		num = mant;
	else if (exp > 291)
		num = INFINITY;
	else if (exp > 0)
		num = mant * __power_of_10[exp];
	else if (exp > -309)
		num = mant / __power_of_10[-exp];
	else if (exp > -324 - figures)
		num = mant / __power_of_10[-exp - 308] / __power_of_10[308];
	else
		num = 0.0;

	return sign ? -num : num;
}

static int __parse_json_number2(const char *cursor, const char **end,
							   double *num)
{
	const char *integer = cursor;
	const char *fraction = "";
	int exp = 0;
	int sign;

	if (*cursor == '-')
		cursor++;

	if (!isdigit(*cursor))
		return -2;

	if (*cursor == '0' && isdigit(cursor[1]))
		return -2;

	cursor++;
	while (isdigit(*cursor))
		cursor++;

	if (*cursor == '.')
	{
		cursor++;
		fraction = cursor;
		if (!isdigit(*cursor))
			return -2;

		cursor++;
		while (isdigit(*cursor))
			cursor++;
	}

	if (*cursor == 'E' || *cursor == 'e')
	{
		cursor++;
		sign = (*cursor == '-');
		if (sign || *cursor == '+')
			cursor++;

		if (!isdigit(*cursor))
			return -2;

		exp = *cursor - '0';
		cursor++;
		while (isdigit(*cursor) && exp < 2000000)
		{
			exp *= 10;
			exp += *cursor - '0';
			cursor++;
		}

		while (isdigit(*cursor))
			cursor++;

		if (sign)
			exp = -exp;
	}

	if (cursor - integer > 1000000)
		return -2;

	*num = __evaluate_json_number2(integer, fraction, exp);
	*end = cursor;
	return 0;
}

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/times.h>
#//include "fast_double_parser.h"


int main()
{
	static char buf[1024*1024];
	size_t len;
	char *end;
	double d;
	int i;
	clock_t from, to;

	while (scanf("%s", buf) == 1)
	{
		len = strlen(buf);
		if (strlen(buf) != 0)
		{
			d = apple_strtod(buf, &end);
			printf("%.50e, strlen = %lu, nlen = %lu. (Apple)\n", d, len, end - buf);

			d = strtod(buf, &end);
			printf("%.50e, strlen = %lu, nlen = %lu. (libc)\n", d, len, end - buf);

			if (__parse_json_number(buf, (const char **)&end, &d) == 0) 
				printf("%.50e, strlen = %lu, nlen = %lu. (Json)\n", d, len, end - buf);
			else
				printf("Error with JSON\n");

			if (__parse_json_number2(buf, (const char **)&end, &d) == 0) 
				printf("%.50e, strlen = %lu, nlen = %lu. (Json2)\n", d, len, end - buf);
			else
				printf("Error with JSON\n");
/*
			end = (char *)fast_double_parser::parse_number(buf, &d);
			if (!end)
				end = buf;
	
			printf("%.50e, strlen = %lu, nlen = %lu. (Fast)\n", d, len, end - buf);*/
			#define N	100000000
/*
			from = times(NULL);
			for (i = 0; i < N; i++)
				d = apple_strtod(buf, &end);
			to = times(NULL);
			printf("Apple time: %lf, %.20lf\n", (to-from)/100.0, 0.0);
*/
			from = times(NULL);
			for (i = 0; i < N; i++)
				d = strtod(buf, &end);
			to = times(NULL);
			printf("libc  time: %lf, %.20lf\n", (to-from)/100.0, 0.0);

			from = times(NULL);
			for (i = 0; i < N; i++)
				__parse_json_number(buf, (const char **)&end, &d);
			to = times(NULL);
			printf("Json  time: %lf, %.20lf\n", (to-from)/100.0, 0.0);

			from = times(NULL);
			for (i = 0; i < N; i++)
				__parse_json_number2(buf, (const char **)&end, &d);
			to = times(NULL);
			printf("Json2 time: %lf, %.20lf\n", (to-from)/100.0, 0.0);
	
/*
			from = times(NULL);
			for (i = 0; i < N; i++)
				fast_double_parser::parse_number(buf, &d);
			to = times(NULL);
			printf("Fast  time: %lf, %.20lf\n", (to-from)/100.0, 0.0);
			*/
		}
		else
			break;
	}
	return 0;
}

