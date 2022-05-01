// from https://www.shadertoy.com/view/4s2GRR
// Inspired by http://stackoverflow.com/questions/6030814/add-fisheye-effect-to-images-at-runtime-using-opengl-es
uniform float Factor;

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 p = fragCoord.xy / iResolution.x;
    float prop = iResolution.x / iResolution.y; //screen proportion
    vec2 m = vec2(0.5, 0.5 / prop); //center coords
    vec2 d = p - m; //vector from center to current fragment
    float r = sqrt(dot(d, d)); // distance of pixel from center

    // amount of effect
    float power = ( 2.0 * 3.141592 / (2.0 * sqrt(dot(m, m))) ) * (Factor-0.5);

    float bind; //radius of 1:1 effect
    if (power > 0.0) bind = sqrt(dot(m, m)); //stick to corners
    else {if (prop < 1.0) bind = m.x; else bind = m.y;} //stick to borders

    //Weird formulas
    vec2 uv;
    if (power > 0.0)//fisheye
        uv = m + normalize(d) * tan(r * power) * bind / tan( bind * power);
    else if (power < 0.0)//antifisheye
        uv = m + normalize(d) * atan(r * -power * 10.0) * bind / atan(-power * bind * 10.0);
    else uv = p;//no effect for power = 1.0

    // Second part of cheat for round effect, not elliptical
    vec3 col = texture(iChannel0, vec2(uv.x, uv.y * prop)).xyz;
    fragColor = vec4(col, 1.0);
}
