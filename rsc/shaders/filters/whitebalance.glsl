// From https://www.shadertoy.com/view/Ml2BRD
// and  https://www.shadertoy.com/view/MtcfDr
// By Tynach. 
// Adapted by Bruno Herbelin for vimix

precision highp float;

/*
 * Settings
 */

// Minimum temperature in the range (defined in the standard as 4000)
const float minTemp = 4000.0;

// Maximum temperature in the range (defined in the standard as 25000)
const float maxTemp = 9000.0;

// [0 1] value slider between min and max Temp
uniform float Temperature;

// Color for picked 'grey'
uniform float Red;
uniform float Green;
uniform float Blue;


// Parameters for transfer characteristics (gamma curves)
struct transfer {
	// Exponent used to linearize the signal
	float power;

	// Offset from 0.0 for the exponential curve
	float off;

	// Slope of linear segment near 0
	float slope;

	// Values below this are divided by slope during linearization
	float cutoffToLinear;

	// Values below this are multiplied by slope during gamma correction
	float cutoffToGamma;
};

// Parameters for a colorspace
struct rgb_space {
	// Chromaticity coordinates (xyz) for Red, Green, and Blue primaries
	mat4 primaries;

	// Chromaticity coordinates (xyz) for white point
	vec4 white;

	// Linearization and gamma correction parameters
	transfer trc;
};


/*
 * Preprocessor 'functions' that help build colorspaces as constants
 */

// Turns 6 chromaticity coordinates into a 3x3 matrix
#define Primaries(r1, r2, g1, g2, b1, b2)\
	mat4(\
	(r1), (r2), 1.0 - (r1) - (r2), 0,\
	(g1), (g2), 1.0 - (g1) - (g2), 0,\
	(b1), (b2), 1.0 - (b1) - (b2), 0,\
	0, 0, 0, 1)

// Creates a whitepoint's xyz chromaticity coordinates from the given xy coordinates
#define White(x, y)\
	vec4(vec3((x), (y), 1.0 - (x) - (y))/(y), 1)

// Automatically calculate the slope and cutoffs for transfer characteristics
#define Transfer(po, of)\
transfer(\
	(po),\
	(of),\
	(pow((po)*(of)/((po) - 1.0), 1.0 - (po))*pow(1.0 + (of), (po)))/(po),\
	(of)/((po) - 1.0),\
	(of)/((po) - 1.0)*(po)/(pow((po)*(of)/((po) - 1.0), 1.0 - (po))*pow(1.0 + (of), (po)))\
)

// Creates a scaling matrix using a vec4 to set the xyzw scalars
#define diag(v)\
	mat4(\
	(v).x, 0, 0, 0,\
	0, (v).y, 0, 0,\
	0, 0, (v).z, 0,\
	0, 0, 0, (v).w)

// Creates a conversion matrix that turns RGB colors into XYZ colors
#define rgbToXyz(space)\
	(space.primaries*diag(inverse((space).primaries)*(space).white))

// Creates a conversion matrix that turns XYZ colors into RGB colors
#define xyzToRgb(space)\
	inverse(rgbToXyz(space))


/*
 * Chromaticities for RGB primaries
 */


// Rec. 709 (HDTV) and sRGB primaries
const mat4 primaries709 = Primaries(
	0.64, 0.33,
	0.3, 0.6,
	0.15, 0.06
);


// Same as above, but in fractional form
const mat4 primariesLms = Primaries(
	194735469.0/263725741.0, 68990272.0/263725741.0,
	141445123.0/106612934.0, -34832189.0/106612934.0,
	36476327.0/229961670.0, 0.0
);


/*
 * Chromaticities for white points
 */

// Standard illuminant E (also known as the 'equal energy' white point)
const vec4 whiteE = White(1.0/3.0, 1.0/3.0);

// Standard Illuminant D65 according to the Rec. 709 and sRGB standards
const vec4 whiteD65S = White(0.3127, 0.3290);

/*
 * Gamma curve parameters
 */
// Linear gamma
const transfer gam10 = transfer(1.0, 0.0, 1.0, 0.0, 0.0);
// Gamma for sRGB
const transfer gamSrgb = transfer(2.4, 0.055, 12.92, 0.04045, 0.0031308);

/*
 * RGB Colorspaces
 */
// sRGB (mostly the same as Rec. 709, but different gamma and full range values)
const rgb_space Srgb = rgb_space(primaries709, whiteD65S, gamSrgb);

// Lms primaries, balanced against equal energy white point
const rgb_space LmsRgb = rgb_space(primariesLms, whiteE, gam10);


/*
 * Colorspace conversion functions
 */

// Converts RGB colors to a linear light scale
vec4 toLinear(vec4 color, const transfer trc)
{
	bvec4 cutoff = lessThan(color, vec4(trc.cutoffToLinear));
	bvec4 negCutoff = lessThanEqual(color, vec4(-1.0*trc.cutoffToLinear));
	vec4 higher = pow((color + trc.off)/(1.0 + trc.off), vec4(trc.power));
	vec4 lower = color/trc.slope;
	vec4 neg = -1.0*pow((color - trc.off)/(-1.0 - trc.off), vec4(trc.power));

	vec4 result = mix(higher, lower, cutoff);
	return mix(result, neg, negCutoff);
}

// Gamma-corrects RGB colors to be sent to a display
vec4 toGamma(vec4 color, const transfer trc)
{
	bvec4 cutoff = lessThan(color, vec4(trc.cutoffToGamma));
	bvec4 negCutoff = lessThanEqual(color, vec4(-1.0*trc.cutoffToGamma));
	vec4 higher = (1.0 + trc.off)*pow(color, vec4(1.0/trc.power)) - trc.off;
	vec4 lower = color*trc.slope;
	vec4 neg = (-1.0 - trc.off)*pow(-1.0*color, vec4(1.0/trc.power)) + trc.off;

	vec4 result = mix(higher, lower, cutoff);
	return mix(result, neg, negCutoff);
}

// Scales a color to the closest in-gamut representation of that color
vec4 gamutScale(vec4 color, float luma)
{
	float low = min(color.r, min(color.g, min(color.b, 0.0)));
	float high = max(color.r, max(color.g, max(color.b, 1.0)));

	float lowScale = low/(low - luma);
	float highScale = max((high - 1.0)/(high - luma), 0.0);
	float scale = max(lowScale, highScale);
	color.rgb += scale*(luma - color.rgb);

	return color;
}
// Calculate Standard Illuminant Series D light source XYZ values
vec4 temperatureToXyz(float temperature)
{
	// Calculate terms to be added up. Since only the coefficients aren't
	// known ahead of time, they're the only thing determined by mix()
	float x = dot(mix(
		vec4(0.244063, 99.11, 2967800.0, -4607000000.0),
		vec4(0.23704, 247.48, 1901800.0, -2006400000.0),
		bvec4(temperature > 7000.0)
	)/vec4(1, temperature, pow(temperature, 2.0), pow(temperature, 3.0)), vec4(1));

	return White(x, -3.0*pow(x, 2.0) + 2.87*x - 0.275);
}


// XYZ conversion matrices for the display colorspace
const mat4 toXyz = rgbToXyz(Srgb);
const mat4 toRgb = xyzToRgb(Srgb);

// LMS conversion matrices for white point adaptation
const mat4 toLms = xyzToRgb(LmsRgb);
const mat4 frLms = rgbToXyz(LmsRgb);


// Converts from one RGB colorspace to another, output as linear light
vec4 convert(vec4 color, vec4 whiteNew)
{
	color = toXyz*color;

	color= toLms*color;
	color = diag(((toLms*Srgb.white)/(toLms*whiteNew)))*color;
	color = inverse(toLms)*color;
	float luma = color.y;

	color = toRgb*color;
	color = gamutScale(color, luma);

	return color;
}

// Main function
void mainImage(out vec4 color, in vec2 coord)
{
    	vec2 uv = coord.xy / iResolution.xy;
    	vec4 texColor = texture(iChannel0, uv);
    
	texColor = toLinear(texColor, Srgb.trc);
    
	// Calculate temperature conversion
	vec4 convWhite = temperatureToXyz(Temperature*(maxTemp - minTemp) + minTemp);
	mat4 adapt = toRgb*frLms*diag((toLms*convWhite)/(toLms*Srgb.white))*toLms*toXyz;
	texColor = adapt*texColor;

	// Darken values to fit within gamut (for texture)
	vec4 newWhite = adapt*vec4(1);
	texColor.rgb /= max(newWhite.r, max(newWhite.g, newWhite.b));
	color = texColor;
	
	// color balance by color picker
	vec4 whiteNew = vec4( Red, Green, Blue, 1.0);
	whiteNew = toXyz*whiteNew;
	whiteNew /= dot(whiteNew, vec4(1.0));
	whiteNew /= whiteNew.y;
	color = convert(color, whiteNew);

	// Convert to display gamma curve
	color = toGamma(color, Srgb.trc);
}


