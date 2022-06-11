// Blue Noise Stippling 2 https://www.shadertoy.com/view/ldyXDd by FabriceNeyret2
// simplified version of joeedh's https://www.shadertoy.com/view/Md3GWf
// see also https://www.shadertoy.com/view/MdtGD7
// --- checkerboard noise : to decorelate the pattern between size x size tiles
// Adapted by Bruno Herbelin for vimix

// simple x-y decorrelated noise seems enough
#define stepnoise0(p, size) rnd( floor(p/size)*size )
#define rnd(U) fract(sin( 1e3*(U)*mat2(1,-7.131, 12.9898, 1.233) )* 43758.5453)

//   joeedh's original noise (cleaned-up)
vec2 stepnoise(vec2 p, float size) {
    p = floor((p+10.)/size)*size;          // is p+10. useful ?
    p = fract(p*.1) + 1. + p*vec2(2,3)/1e4;
    p = fract( 1e5 / (.1*p.x*(p.y+vec2(0,1)) + 1.) );
    p = fract( 1e5 / (p*vec2(.1234,2.35) + 1.) );
    return p;
}

// --- stippling mask  : regular stippling + per-tile random offset + tone-mapping
float mask(vec2 p) {
#define SEED1 1.705
#define DMUL  8.12235325       // are exact DMUL and -.5 important ?
    p += ( stepnoise0(p, 5.5) - .5 ) *DMUL;   // bias [-2,2] per tile otherwise too regular
    float f = fract( p.x*SEED1 + p.y/(SEED1+.15555) ); //  weights: 1.705 , 0.5375
  //return f;  // If you want to skeep the tone mapping
    f *= 1.03; //  to avoid zero-stipple in plain white ?
    // --- indeed, is a tone mapping ( equivalent to do the reciprocal on the image, see tests )
    // returned value in [0,37.2] , but < 0.57 with P=50%
    return  (pow(f, 150.) + 1.3*f ) / 2.3; // <.98 : ~ f/2, P=50%  >.98 : ~f^150, P=50%
}                                          // max = 37.2, int = 0.55

// --- for ramp at screen bottom
#define tent(f) ( 1. - abs(2.*fract(f)-1.) )
// --- fetch luminance( texture (pixel + offset) )
#define s(x,y) dot( texture(iChannel0, (U+vec2(x,y))/R ), vec4(.3,.6,.1,0) ) // luminance

void mainImage( out vec4 O, vec2 U )
{
    vec2 R = iResolution.xy; O-=O;
    // --- fetch video luminance and enhance the contrast
    float f =  s(-1,-1) + s(-1,0) + s(-1,1)
             + s( 0,-1) +         + s( 0,1)
             + s( 1,-1) + s( 1,0) + s( 1,1),
         f0 = s(0,0);
    f = ( .125*f + 8.*f0 ) / 6.;

    f = f0 - ( f-f0 ) * 0.5;

    // --- stippling
    O += step(mask(U), f);
}

