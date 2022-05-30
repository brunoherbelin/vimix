#define SEED uvec4(0x5C995C6Du, 0x6A3C6A57u, 0xC65536CBu, 0x3563995Fu)
const uint lcgM = 2891336453u;// ideal for 32 bits with odd c
uint  asuint2(float x) { return x == 0.0 ? 0u : floatBitsToUint(x); }
uvec2 asuint2(vec2 x) { return uvec2(asuint2(x.x ), asuint2(x.y)); }
uvec3 asuint2(vec3 x) { return uvec3(asuint2(x.xy), asuint2(x.z)); }
uvec3 pcg3Mix(uvec3 h) {
    h.x += h.y * h.z;
    h.y += h.z * h.x;
    h.z += h.x * h.y;
    return h;
}
uvec3 pcg3Permute(uvec3 h) {
    h = pcg3Mix(h);
    h ^= h >> 16u;
    return pcg3Mix(h);
}
uvec3 pcg3(uvec3 h, uint seed) {
    uvec3 c = (seed << 1u) ^ SEED.xyz;

    return pcg3Permute(h * lcgM + c);
}
float Float11(uint x) { return float(int(x)) * (1.0 / 2147483648.0); }
float Hash11(vec2  v, uint seed) { return Float11(pcg3(asuint2(vec3(v, 0.0)), seed).x); }
float Float01(uint x) { return float( x ) * (1.0 / 4294967296.0); }
float Hash01(vec2  v, uint seed) { return Float01(pcg3(asuint2(vec3(v, 0.0)), seed).x); }