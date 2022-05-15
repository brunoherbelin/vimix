////////// Original code credits: //////////
// by Jan Eric Kyprianidis <www.kyprianidis.com>
// https://code.google.com/p/gpuakf/source/browse/glsl/kuwahara.glsl
////////////////////////////////////////////
// This shader is modified to be compatible with LOVE2D's quirky shader engine!
// Modified by Christian D. Madsen
// https://github.com/strangewarp/LoveAnisotropicKuwahara

uniform float Radius;

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec4 outpix;

    int rad = int(Radius * 10.0);

    vec2 uv_step = 1.0 / iResolution.xy;
    vec2 uv = fragCoord.xy / iResolution.xy;
    float n = float((rad + 1) * (rad + 1));

    ////////////////////////////////////////////////////////////////////////////////
    // Bilateral difference-check, to speed up the processing of large flat areas //
    ////////////////////////////////////////////////////////////////////////////////
    vec3 compare = texture(iChannel0, uv).rgb;
    bool diffpix = false;
    for (int j = -rad; j <= rad; ++j) {
        if (texture(iChannel0, uv + vec2(0,j) * uv_step).rgb != compare) {
            diffpix = true;
            break;
        }
    }
    if (!diffpix) {
        for (int i = -rad; i <= rad; ++i) {
            if (texture(iChannel0, uv + vec2(i,0) * uv_step).rgb != compare) {
                diffpix = true;
                break;
            }
        }
    }
    if (!diffpix) {
        outpix = vec4( texture(iChannel0, uv).rgb, 1.0);
    }
    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////
	else {
	    vec3 m[4];
	    vec3 s[4];
	    for (int k = 0; k < 4; ++k) {
	        m[k] = vec3(0.0);
	        s[k] = vec3(0.0);
	    }
	    float R = 0.1;
    	float D = length(vec2(rad));
	
	    for (int j = -rad; j <= 0; ++j)  {
	        for (int i = -rad; i <= 0; ++i)  {
                vec2 delta = vec2(i, j);
	            vec3 c = texture(iChannel0, uv + delta * smoothstep(R, D, length(delta)) * uv_step).rgb;
	            m[0] += c;
	            s[0] += c * c;
	        }
	    }
	
	    for (int j = -rad; j <= 0; ++j)  {
	        for (int i = 0; i <= rad; ++i)  {
                vec2 delta = vec2(i, j);
	            vec3 c = texture(iChannel0, uv + delta * smoothstep(R, D, length(delta)) * uv_step).rgb;
	            m[1] += c;
	            s[1] += c * c;
	        }
	    }
	
	    for (int j = 0; j <= rad; ++j)  {
	        for (int i = 0; i <= rad; ++i)  {
                vec2 delta = vec2(i, j);
	            vec3 c = texture(iChannel0, uv + delta * smoothstep(R, D, length(delta)) * uv_step).rgb;
	            m[2] += c;
	            s[2] += c * c;
	        }
	    }
	
	    for (int j = 0; j <= rad; ++j)  {
	        for (int i = -rad; i <= 0; ++i)  {
                vec2 delta = vec2(i, j);
	            vec3 c = texture(iChannel0, uv + delta * smoothstep(R, D, length(delta)) * uv_step).rgb;
	            m[3] += c;
	            s[3] += c * c;
	        }
	    }
	
	    float min_sigma2 = 1e+2;
	    for (int k = 0; k < 4; ++k) {
	        m[k] /= n;
	        s[k] = abs(s[k] / n - m[k] * m[k]);
	        float sigma2 = s[k].r + s[k].g + s[k].b;
	        if (sigma2 < min_sigma2) {
	            min_sigma2 = sigma2;
	            outpix = vec4(m[k], 1.0);
	        }
	    }
    }

    fragColor = outpix;
}

