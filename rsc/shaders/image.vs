#version 330 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec4 color;
layout (location = 2) in vec2 texCoord;

out vec4 vertexColor;
out vec2 vertexUV;

uniform mat4 modelview;
uniform mat4 projection;
uniform mat4 iNodes;

void main()
{
    // BARYCENTRIC COORDINATES
    // compute the barycentric coordinates in a square {P0 P1 P2 P3} for the given vertex
    vec4 coef = abs (vec4( 1.0 - position.x - position.y + position.y * position.x,
                      1.0 - position.x + position.y - position.y * position.x,
                      1.0 + position.x - position.y - position.y * position.x,
                      1.0 + position.x + position.y + position.y * position.x));
    vec4 corners = coef / (coef[0] + coef[1] + coef[2] + coef[3]);

    // delinearizing coordinates with attractor parameter z
    coef[0] = pow( coef[0], 1.0 + iNodes[0].z);
    coef[1] = pow( coef[1], 1.0 + iNodes[1].z);
    coef[2] = pow( coef[2], 1.0 + iNodes[2].z);
    coef[3] = pow( coef[3], 1.0 + iNodes[3].z);

    // calculate coordinates of point in distorted square {A0 A1 A2 A3}
    vec2 P= coef[0] * vec2(iNodes[0].x - 1.0, iNodes[0].y - 1.0) +
            coef[1] * vec2(iNodes[1].x - 1.0, iNodes[1].y + 1.0) +
            coef[2] * vec2(iNodes[2].x + 1.0, iNodes[2].y - 1.0) +
            coef[3] * vec2(iNodes[3].x + 1.0, iNodes[3].y + 1.0);
    vec3 pos = vec3( P / (coef[0] + coef[1] + coef[2] + coef[3]), position.z );

    // Rounding corners with attractor parameter w
    vec3 rounded = ( smoothstep(0.0, 1.0, length(position.xy)) ) * normalize(vec3(position.xy, 0.01));
    pos = mix( pos, rounded, corners.x * iNodes[0].w);
    pos = mix( pos, rounded, corners.y * iNodes[1].w);
    pos = mix( pos, rounded, corners.z * iNodes[2].w);
    pos = mix( pos, rounded, corners.w * iNodes[3].w);

    // output
    gl_Position = projection * ( modelview * vec4(pos, 1.0) );
    vertexColor = color;
    vertexUV    = texCoord;
}

