StructuredBuffer<uint> cells : register(t0);

// cbuffer members are globals in HLSL: write gridWidth, not GridConstants.gridWidth
cbuffer GridConstants : register(b0)
{
    uint gridWidth;
    uint gridHeight;
    float viewportWidth;
    float viewportHeight;
};

static const float4 LetterboxColor = float4(0.0f, 0.0f, 0.0f, 1.0f);

float3 MaterialColor(uint material)
{
    if (material == 1) // sand
    {
        return float3(0.86f, 0.72f, 0.42f);
    }
    if (material == 2) // wall
    {
        return float3(0.45f, 0.45f, 0.48f);
    }
    return float3(0.08f, 0.09f, 0.11f); // empty
}

float4 PSMain(float4 position : SV_POSITION) : SV_TARGET
{
    // Pixels per cell: the largest square cell size that fits the whole grid in the window
    const float scale = min(viewportWidth / gridWidth, viewportHeight / gridHeight);
    // Top-left pixel of the centered grid
    const float2 offset = (float2(viewportWidth, viewportHeight) - float2(gridWidth, gridHeight) * scale) * 0.5f;

    // Stay in float until the bounds check: a negative value cast to uint wraps to a huge number
    const float2 gridPos = (position.xy - offset) / scale;
    if (any(gridPos < 0.0f) || any(gridPos >= float2(gridWidth, gridHeight)))
    {
        return LetterboxColor;
    }

    const uint2 cellPos = uint2(gridPos);
    // Cell layout (see sim/Material.h): bits 0-7 material, bits 8-15 per-grain shade
    const uint cell = cells[cellPos.y * gridWidth + cellPos.x];
    const uint material = cell & 0xFF;
    const float shade = ((cell >> 8) & 0xFF) / 255.0f;

    float3 color = MaterialColor(material);
    if (material != 0)
    {
        color *= lerp(0.85f, 1.15f, shade);
    }
    return float4(color, 1.0f);
}
