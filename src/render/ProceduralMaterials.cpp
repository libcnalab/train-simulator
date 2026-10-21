#include "trainsim/render/ProceduralMaterials.hpp"

#include "CnaEngine/Graphics/Image.hpp"

#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace TrainSim::Render {

ProceduralImage makeRoofDetailImage(int size)
{
    if (size<16 || size>1024) throw std::invalid_argument("Invalid roof image size");
    ProceduralImage image;
    image.width=image.height=size;
    image.pixels.reserve(static_cast<std::size_t>(size)*static_cast<std::size_t>(size));
    for (int y=0;y<size;++y)
        for (int x=0;x<size;++x)
        {
            const float py=static_cast<float>(y)*4/static_cast<float>(size);
            const int row=static_cast<int>(py);
            const float fy=py-static_cast<float>(row);
            const float px=static_cast<float>(x)*4/static_cast<float>(size)
                +static_cast<float>(row%2)*0.5f;
            const int column=static_cast<int>(px);
            const float fx=px-static_cast<float>(column);
            std::uint32_t tileHash=static_cast<std::uint32_t>(row*17+(column%4)*101+23)*0x9e3779b9U;
            tileHash^=tileHash>>16U;
            const float tileTint=(static_cast<float>(tileHash%9U)-4)*2.5f;
            const float curve=13*std::sin(fx*3.141592654f);
            const float joint=(fy<0.065f ? 30.0f : 0.0f)+(fx<0.04f ? 17.0f : 0.0f);
            const int value=std::clamp(static_cast<int>(224+curve+tileTint-joint),170,250);
            image.pixels.emplace_back(value,value,value,255);
        }
    return image;
}

ProceduralImage makeFoliageDetailImage(int size)
{
    if (size < 8 || size > 1024) throw std::invalid_argument("Invalid foliage image size");
    ProceduralImage image;
    image.width = image.height = size;
    image.pixels.reserve(static_cast<std::size_t>(size)*static_cast<std::size_t>(size));
    constexpr float tau = 6.283185307f;
    // Periodic overlapping leaf-shaped highlights, not white noise. All texture edges wrap;
    // opaque modulation avoids transparency sorting and keeps crown interiors visually solid.
    for (int y=0;y<size;++y)
        for (int x=0;x<size;++x)
        {
            const float u=static_cast<float>(x)/static_cast<float>(size)*8;
            const float v=static_cast<float>(y)/static_cast<float>(size)*8;
            const int cellX=static_cast<int>(u), cellY=static_cast<int>(v);
            float leaf=0;
            for (int dy=-1;dy<=1;++dy)
                for (int dx=-1;dx<=1;++dx)
                {
                    const int cx=cellX+dx, cy=cellY+dy;
                    const float phase=static_cast<float>(((cx+8)%8)*13+((cy+8)%8)*31);
                    const float angle=std::sin(phase*1.73f)*3;
                    const float px=u-(static_cast<float>(cx)+0.5f+0.22f*std::sin(phase));
                    const float py=v-(static_cast<float>(cy)+0.5f+0.22f*std::cos(phase*2.1f));
                    const float a=(px*std::cos(angle)+py*std::sin(angle))/0.58f;
                    const float b=(-px*std::sin(angle)+py*std::cos(angle))/0.28f;
                    const float shape=std::max(0.0f,1-a*a-b*b);
                    leaf=std::max(leaf,std::sqrt(shape)*(0.78f+0.22f*std::sin(phase+1)));
                }
            const float cluster=std::sin(tau*u/8+0.8f*std::sin(tau*v/8));
            const int value=std::clamp(static_cast<int>(198+48*leaf+8*cluster),180,255);
            image.pixels.emplace_back(value,value,value,255);
        }
    return image;
}

ProceduralImage makeLeafSprayImage(int size)
{
    if (size<16 || size>1024) throw std::invalid_argument("Invalid leaf spray image size");
    ProceduralImage image{size,size,{}};
    image.pixels.reserve(static_cast<std::size_t>(size)*static_cast<std::size_t>(size));
    for (int y=0;y<size;++y)
        for (int x=0;x<size;++x)
        {
            const float u=(static_cast<float>(x)+0.5f)/static_cast<float>(size);
            const float v=(static_cast<float>(y)+0.5f)/static_cast<float>(size);
            float coverage=0, detail=0;
            for (int leaf=0;leaf<26;++leaf)
            {
                const float index=static_cast<float>(leaf);
                const float reach=0.34f*std::sqrt((index+0.5f)/26.0f);
                const float cx=0.5f+reach*std::cos(index*2.39996f);
                const float cy=0.5f+reach*std::sin(index*2.39996f);
                const float angle=index*1.73f;
                const float dx=u-cx, dy=v-cy;
                const float a=(dx*std::cos(angle)+dy*std::sin(angle))/0.09f;
                const float b=(-dx*std::sin(angle)+dy*std::cos(angle))/0.035f;
                const float shape=1.0f-a*a-b*b;
                coverage=std::max(coverage,std::clamp(shape*12.0f,0.0f,1.0f));
                if (shape>0) detail=std::max(detail,shape);
            }
            if (u<0.03f || u>0.97f || v<0.03f || v>0.97f) coverage=0;
            const int value=static_cast<int>(205+45*detail);
            // Straight-alpha RGB stays leaf-coloured even outside the silhouette, avoiding
            // dark fringes during filtering. Alpha testing supplies holes, not alpha blending.
            image.pixels.emplace_back(value,value,value,static_cast<int>(255*coverage));
        }
    return image;
}

ProceduralImage makeNeedleSprayImage(int size)
{
    if (size<16 || size>1024) throw std::invalid_argument("Invalid needle spray image size");
    struct Stroke { float ax,ay,bx,by,radius; };
    std::vector<Stroke> strokes;
    strokes.reserve(325);
    strokes.push_back({0.5f,0.05f,0.5f,0.95f,0.006f});
    for (int twig=0;twig<12;++twig)
        for (float side : {-1.0f,1.0f})
        {
            const float ordinal=static_cast<float>(twig);
            const float y=0.13f+ordinal*0.063f+0.008f*std::sin(ordinal*2.3f+side);
            const float reach=(0.18f+0.15f*std::sin(y*3.14159265f))*(0.94f+0.06f*std::sin(ordinal*1.7f+side));
            const float endX=0.5f+side*reach,endY=y+0.065f;
            strokes.push_back({0.5f,y,endX,endY,0.004f});
            for (int needle=1;needle<=6;++needle)
                for (float direction : {-1.0f,1.0f})
                {
                    const float t=static_cast<float>(needle)/7;
                    const float x=0.5f+side*reach*t,ny=y+0.065f*t;
                    const float length=0.045f+0.012f*std::sin(ordinal+static_cast<float>(needle)*1.8f);
                    strokes.push_back({x,ny,x+side*0.035f,ny+direction*length,0.010f});
                }
        }
    ProceduralImage image{size,size,{}};
    image.pixels.reserve(static_cast<std::size_t>(size)*static_cast<std::size_t>(size));
    for (int y=0;y<size;++y)
        for (int x=0;x<size;++x)
        {
            const float u=(static_cast<float>(x)+0.5f)/static_cast<float>(size);
            const float v=(static_cast<float>(y)+0.5f)/static_cast<float>(size);
            float coverage=0;
            for (const auto& line : strokes)
            {
                const float dx=line.bx-line.ax,dy=line.by-line.ay;
                const float t=std::clamp(((u-line.ax)*dx+(v-line.ay)*dy)/(dx*dx+dy*dy),0.0f,1.0f);
                const float distance=std::hypot(u-line.ax-t*dx,v-line.ay-t*dy);
                coverage=std::max(coverage,std::clamp((line.radius-distance)*static_cast<float>(size)+0.5f,0.0f,1.0f));
            }
            const int shade=static_cast<int>(222+25*(1-v));
            image.pixels.emplace_back(shade,shade,shade,static_cast<int>(255*coverage));
        }
    return image;
}

ProceduralImage makeNeedleDetailImage(int size)
{
    if (size<8 || size>1024) throw std::invalid_argument("Invalid needle image size");
    ProceduralImage image;
    image.width=image.height=size;
    image.pixels.reserve(static_cast<std::size_t>(size)*static_cast<std::size_t>(size));
    constexpr float tau=6.283185307f;
    for (int y=0;y<size;++y)
        for (int x=0;x<size;++x)
        {
            const float u=static_cast<float>(x)/static_cast<float>(size);
            const float v=static_cast<float>(y)/static_cast<float>(size);
            // Two periodic oblique families form fine needle sprays. Lower-frequency
            // modulation supplies clumps; opaque neutral values preserve the tree palette.
            const float a=std::pow(std::max(0.0f,std::cos(tau*(23*u+11*v)
                +0.65f*std::sin(tau*3*v))),8.0f);
            const float b=std::pow(std::max(0.0f,std::cos(tau*(17*u-13*v)
                +0.60f*std::sin(tau*4*u))),8.0f);
            const float clump=std::sin(tau*(3*u+2*v))*std::cos(tau*(u-3*v));
            const int value=std::clamp(static_cast<int>(210+27*std::max(a,b)+15*clump),180,255);
            image.pixels.emplace_back(value,value,value,255);
        }
    return image;
}

double ruralParcelBoundary(std::int64_t parcel,double side)
{
    // Whole 50 m terrain cells keep every boundary on a cell edge, where the subdivided field
    // surface already has its narrow margin samples. Shifts up to 150 m keep parcels ordered.
    auto h=static_cast<std::uint64_t>(parcel)*0x9e3779b97f4a7c15ULL
         ^(side<0 ? 0x2545f4914f6cdd1dULL : 0x632be59bd9b4e019ULL);
    h=(h^(h>>30))*0xbf58476d1ce4e5b9ULL;
    h=(h^(h>>27))*0x94d049bb133111ebULL;
    h^=h>>31;
    return static_cast<double>(parcel)*500.0+50.0*(static_cast<double>(h%7U)-3.0);
}

std::int64_t ruralParcelIndex(double distance,double side)
{
    auto parcel=static_cast<std::int64_t>(std::floor(distance/500.0));
    if (distance<ruralParcelBoundary(parcel,side)) --parcel;
    else if (distance>=ruralParcelBoundary(parcel+1,side)) ++parcel;
    return parcel;
}

namespace {
// Palette slot of a field parcel, shared by its colour and by its surface working (ENV-130).
std::uint64_t ruralParcelSlot(double distance,double lateral)
{
    const double across=std::abs(lateral);
    auto identity=static_cast<std::uint64_t>(ruralParcelIndex(distance,lateral));
    identity^=lateral<0 ? 0x9e3779b97f4a7c15ULL : 0xbf58476d1ce4e5b9ULL;
    if (across>=100) identity^=0x94d049bb133111ebULL;
    identity=(identity^(identity>>30))*0xbf58476d1ce4e5b9ULL;
    identity=(identity^(identity>>27))*0x94d049bb133111ebULL;
    return (identity^(identity>>31))%6U;
}
}

bool ruralParcelArable(double distance,double lateral)
{
    const double across=std::abs(lateral);
    if (across<26 || across>260) return false;
    const auto slot=ruralParcelSlot(distance,lateral);
    // Stubble and ploughed soil carry tramlines; pasture, hay and fallow do not.
    return slot==2U || slot==3U;
}

Color ruralParcelTint(double distance,double lateral)
{
    const double across=std::abs(lateral);
    if (across<8.8) return {81,116,65,255};
    if (across<12) return {65,96,57,255};
    if (across<26) return {91,133,72,255};
    if (across>600) return {105,124,102,255};
    if (across>260) return {91,119,88,255};
    // Parcel edges match the irregular per-side hedge boundaries and existing terrain bands.
    // Pasture, mown hay, stubble and fallow tones replace unrelated per-mesh-band stripes.
    const Color palette[]={{91,133,72,255},{104,129,70,255},{139,137,83,255},
                           {137,119,78,255},{105,119,64,255},{112,108,80,255}};
    return palette[ruralParcelSlot(distance,lateral)];
}

Color ruralTerrainSurfaceTint(double distance,double lateral)
{
    const double across=std::abs(lateral);
    if (across<=26) return ruralParcelTint(distance, std::copysign(std::min(across,25.999),lateral));
    if (across>=260) return ruralParcelTint(distance,std::copysign(std::max(across,260.001),lateral));
    const auto parcel=ruralParcelIndex(distance,lateral);
    const double edge=std::min({distance-ruralParcelBoundary(parcel,lateral),
                                ruralParcelBoundary(parcel+1,lateral)-distance,
                                across-26,std::abs(across-100),260-across});
    const double blend=std::clamp(edge/3.0,0.0,1.0);
    const double eased=blend*blend*(3-2*blend);
    const auto soil=ruralParcelTint(distance,lateral);
    const double farBlend=std::clamp((across-250)/10.0,0.0,1.0);
    const Color margin(91,static_cast<int>(133-14*farBlend),static_cast<int>(72+16*farBlend),255);
    // Low-amplitude cultivation/soil variation, anchored to route metres rather than chunk IDs.
    const double variation=1+0.025*std::sin(distance*0.023+across*0.19)
        +0.015*std::sin(across*0.47+distance*0.003);
    const auto channel=[&](int a,int b) {
        return std::clamp(static_cast<int>(std::lround(a+(b*variation-a)*eased)),0,255);
    };
    return Color(channel(margin.getRProperty(),soil.getRProperty()),
                 channel(margin.getGProperty(),soil.getGProperty()),
                 channel(margin.getBProperty(),soil.getBProperty()),255);
}

Color urbanGroundTint(double distance, double lateral)
{
    const double variation = 5.0*std::sin(distance/173.0+lateral/57.0)
                           + 3.0*std::sin(distance/61.0-lateral/93.0);
    const double verge = std::clamp((std::abs(lateral)-9.0)/4.0,0.0,1.0);
    const double haze = std::clamp((std::abs(lateral)-260.0)/500.0,0.0,1.0);
    const auto channel = [&](double gravel, double grass, double distant) {
        const double near = gravel+(grass-gravel)*verge+variation;
        return static_cast<int>(std::lround(near+(distant-near)*haze));
    };
    return Color(channel(92,77,107),channel(99,108,124),channel(91,66,101),255);
}

Color terminalGroundTint(double distance,double lateral,double routeLength)
{
    const Color original=urbanGroundTint(distance,lateral);
    const auto smooth=[](double t) { t=std::clamp(t,0.0,1.0); return t*t*(3-2*t); };
    const double endpoint=std::min(std::abs(distance),std::abs(routeLength-distance));
    const double across=std::abs(lateral);
    const double influence=(1-smooth((endpoint-350)/250))
        *smooth((across-10.4)/1.6)*(1-smooth((across-180)/80));
    if (influence<=0) return original;
    const double maintenance=1-smooth((across-26)/14);
    const double managed=smooth((across-40)/15);
    const double variation=4*std::sin(distance/43+lateral/19)
        +3*std::sin(distance/107-lateral/31);
    const auto channel=[&](int source,double rough,double lawn,double gravel) {
        const double green=rough+(lawn-rough)*managed;
        const double target=green+(gravel-green)*maintenance+variation;
        return static_cast<int>(std::lround(source+(target-source)*influence));
    };
    return Color(channel(original.getRProperty(),106,88,124),
                 channel(original.getGProperty(),111,114,120),
                 channel(original.getBProperty(),76,72,103),255);
}

namespace {

std::uint32_t mix(std::uint32_t value) noexcept
{
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    return value ^ (value >> 16U);
}

std::uint32_t texelHash(int x, int y, std::uint32_t seed) noexcept
{
    return mix(static_cast<std::uint32_t>(x) * 0x9e3779b9U
             ^ static_cast<std::uint32_t>(y) * 0x85ebca6bU ^ seed);
}

} // namespace

namespace {
// Mip levels and uploads are cna-engine's (Graphics/Image.hpp): the same truncating 2x2 box
// filter and alpha-test coverage this file used to implement.
CnaEngine::Graphics::Image engineImage(const ProceduralImage& image,const char* what)
{
    if (image.width <= 0 || image.height <= 0
        || image.pixels.size() != static_cast<std::size_t>(image.width*image.height))
        throw std::invalid_argument(what);
    return CnaEngine::Graphics::Image::FromColors(image.width,image.height,image.pixels);
}

ProceduralImage proceduralImage(const CnaEngine::Graphics::Image& image)
{
    return ProceduralImage{image.Width(),image.Height(),image.ToColors()};
}
}

ProceduralImage ProceduralImage::downsampled() const
{
    return proceduralImage(CnaEngine::Graphics::Downsample(
        engineImage(*this,"Cannot downsample an invalid procedural image")));
}

ProceduralImage ProceduralImage::downsampledForAlphaTest(int referenceAlpha,double coverage) const
{
    if (referenceAlpha<1 || referenceAlpha>254 || !std::isfinite(coverage)
        || coverage<0 || coverage>1)
        throw std::invalid_argument("Invalid alpha-test coverage target");
    return proceduralImage(CnaEngine::Graphics::DownsampleForAlphaTest(
        engineImage(*this,"Cannot downsample an invalid procedural image"),referenceAlpha,coverage));
}

ProceduralImage makeTerrainDetailImage(int size)
{
    if (size < 8 || size > 1'024 || (size & (size-1)) != 0)
        throw std::invalid_argument("Terrain detail texture size must be a power of two from 8 to 1024");

    ProceduralImage image;
    image.width = size;
    image.height = size;
    image.pixels.resize(static_cast<std::size_t>(size*size));
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
        {
            // Three deterministic scales avoid both television-static noise and an obvious tiled
            // checker. Sparse darker fibres survive close views; mipmaps turn them into stable
            // broad tonal variation instead of distant shimmer.
            const std::uint32_t fine = texelHash(x,y,0x54455252U);
            const std::uint32_t medium = texelHash(x/8,y/8,0x4d4f5454U);
            const std::uint32_t broad = texelHash(x/24,y/24,0x4649454cU);
            int value = 235
                + static_cast<int>(fine%13U)-6
                + static_cast<int>(medium%17U)-8
                + static_cast<int>(broad%11U)-5;
            if ((fine & 0xffU) < 11U) value -= 25;
            if (((fine >> 8U) & 0xffU) > 248U) value += 12;
            value = std::clamp(value,190,255);
            image.pixels[static_cast<std::size_t>(y*size+x)] = Color(value,value,value,255);
        }
    return image;
}

ProceduralImage makeCloudDetailImage(int size,unsigned variant)
{
    if (size < 8 || size > 1'024 || (size & (size-1)) != 0)
        throw std::invalid_argument(
            "Cloud detail texture size must be a power of two from 8 to 1024");
    ProceduralImage image{size,size,{}};
    image.pixels.resize(static_cast<std::size_t>(size*size));

    struct Lobe { float x; float y; float radiusX; float radiusY; };
    std::array<Lobe,8> lobes = {{
        {-0.69f, 0.15f,0.33f,0.30f},
        {-0.47f,-0.03f,0.44f,0.39f},
        {-0.23f,-0.18f,0.39f,0.52f},
        { 0.02f,-0.27f,0.46f,0.60f},
        { 0.29f,-0.15f,0.43f,0.50f},
        { 0.55f, 0.00f,0.40f,0.38f},
        { 0.76f, 0.17f,0.27f,0.27f},
        { 0.05f, 0.17f,0.82f,0.32f}
    }};
    for (std::size_t i=0;i<lobes.size();++i)
    {
        auto& lobe=lobes[i];
        const float ordinal=static_cast<float>(i);
        if (variant%4U==1U) // Taller, asymmetric fair-weather cumulus.
        {
            lobe.x=lobe.x*0.75f-0.08f;
            lobe.y+=0.13f*std::sin(ordinal*1.7f);
            lobe.radiusX*=0.78f;
            lobe.radiusY*=1.05f;
        }
        else if (variant%4U==2U) // Thin broken horizontal bank.
        {
            lobe.y=0.10f+0.07f*std::sin(ordinal*2.1f);
            lobe.radiusY*=0.40f;
            lobe.radiusX*=0.83f;
        }
        else if (variant%4U==3U) // Ragged small cluster with unequal lobes.
        {
            lobe.x=lobe.x*0.85f+0.10f*std::sin(ordinal*3.0f);
            lobe.y+=0.10f*std::sin(ordinal*2.0f);
            lobe.radiusX*=0.65f;
            lobe.radiusY*=0.75f;
        }
    }
    const auto smoothStep = [](float edge0, float edge1, float value) {
        const float t = std::clamp((value-edge0)/(edge1-edge0),0.0f,1.0f);
        return t*t*(3.0f-2.0f*t);
    };
    const auto valueNoise = [](float u, float v, int cells, std::uint32_t seed) {
        const float gx = u*static_cast<float>(cells);
        const float gy = v*static_cast<float>(cells);
        const int x0 = static_cast<int>(std::floor(gx));
        const int y0 = static_cast<int>(std::floor(gy));
        const float fx = gx-static_cast<float>(x0);
        const float fy = gy-static_cast<float>(y0);
        const float sx = fx*fx*(3.0f-2.0f*fx);
        const float sy = fy*fy*(3.0f-2.0f*fy);
        const auto sample = [seed](int x, int y) {
            return static_cast<float>(texelHash(x,y,seed)&0xffffU)/65'535.0f;
        };
        const float lower = std::lerp(sample(x0,y0),sample(x0+1,y0),sx);
        const float upper = std::lerp(sample(x0,y0+1),sample(x0+1,y0+1),sx);
        return std::lerp(lower,upper,sy);
    };

    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
        {
            const float u = (static_cast<float>(x)+0.5f)/static_cast<float>(size);
            const float v = (static_cast<float>(y)+0.5f)/static_cast<float>(size);
            const float px = u*2.0f-1.0f;
            const float py = v*2.0f-1.0f;
            float signedShape = -2.0f;
            float lobeLight = 0.0f;
            for (const Lobe& lobe : lobes)
            {
                const float dx = (px-lobe.x)/lobe.radiusX;
                const float dy = (py-lobe.y)/lobe.radiusY;
                const float radialSquared=dx*dx+dy*dy;
                const float shape=1.0f-std::sqrt(radialSquared);
                if (shape > signedShape)
                {
                    signedShape=shape;
                    // Approximate a lit, rounded cloud lobe, not just a white silhouette.
                    // Fine density variation below softens the junctions of these lobes.
                    const float depth=std::sqrt(std::max(0.0f,1.0f-radialSquared));
                    lobeLight=std::clamp(-0.32f*dx-0.48f*dy+0.72f*depth,0.0f,1.0f);
                }
            }
            const float broad = valueNoise(u,v,5,0x434c4f55U+variant%4U*17U)-0.5f;
            const float detail = valueNoise(u,v,13,0x44524654U+variant%4U*31U)-0.5f;
            const float density = signedShape+broad*0.19f+detail*0.075f;
            const float alpha = smoothStep(-0.16f,0.12f,density)
                              * smoothStep(0.0f,0.055f,u)
                              * smoothStep(0.0f,0.055f,1.0f-u)
                              * smoothStep(0.0f,0.055f,v)
                              * smoothStep(0.0f,0.055f,1.0f-v);
            const float upperLight = std::clamp(1.0f-v,0.0f,1.0f);
            const int red = std::clamp(static_cast<int>(182.0f+39.0f*upperLight
                +32.0f*lobeLight+broad*18.0f+detail*6.0f),0,247);
            const int green = std::clamp(red+5,0,255);
            const int blue = std::clamp(red+8,0,255);
            image.pixels[static_cast<std::size_t>(y*size+x)] = Color(
                red,green,blue,static_cast<int>(std::lround(alpha*232.0f)));
        }
    return image;
}

ProceduralImage makeCloudAtlasImage(int tileSize)
{
    if (tileSize<8 || tileSize>512 || (tileSize&(tileSize-1))!=0)
        throw std::invalid_argument("Cloud atlas tile size must be a power of two from 8 to 512");
    ProceduralImage atlas{tileSize*2,tileSize*2,{}};
    atlas.pixels.resize(static_cast<std::size_t>(atlas.width*atlas.height));
    for (unsigned variant=0;variant<4;++variant)
    {
        const auto tile=makeCloudDetailImage(tileSize,variant);
        const int left=static_cast<int>(variant%2)*tileSize;
        const int top=static_cast<int>(variant/2)*tileSize;
        for (int y=0;y<tileSize;++y)
            for (int x=0;x<tileSize;++x)
                atlas.pixels[static_cast<std::size_t>((top+y)*atlas.width+left+x)]
                    =tile.pixels[static_cast<std::size_t>(y*tileSize+x)];
    }
    return atlas;
}

namespace {

void requireMaterialSize(int size, const char* material)
{
    if (size < 8 || size > 1'024 || (size & (size-1)) != 0)
        throw std::invalid_argument(
            std::string(material)+" texture size must be a power of two from 8 to 1024");
}

int periodicDistance(int a, int b, int period) noexcept
{
    const int direct = std::abs(a-b);
    return std::min(direct,period-direct);
}

} // namespace

ProceduralImage makeBallastDetailImage(int size)
{
    requireMaterialSize(size,"Ballast detail");
    constexpr int cellSize = 8;
    const int cellCount = std::max(1,size/cellSize);
    ProceduralImage image{size,size,{}};
    image.pixels.resize(static_cast<std::size_t>(size*size));

    // Periodic jittered cells form loose angular aggregate without storing a source bitmap.
    // Full-cell jitter varies stone size, each stone is lit across its own facet so it reads as a
    // tilted solid rather than a flat tile, and only a thin dark crevice separates neighbours;
    // a few stones sit lower in shadow. The mip chain still converges to a calm mid-grey.
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
        {
            int nearest = std::numeric_limits<int>::max();
            int second = std::numeric_limits<int>::max();
            std::uint32_t nearestHash = 0;
            int nearestDx = 0;
            int nearestDy = 0;
            const int baseCellX = x/cellSize;
            const int baseCellY = y/cellSize;
            for (int offsetY = -1; offsetY <= 1; ++offsetY)
                for (int offsetX = -1; offsetX <= 1; ++offsetX)
                {
                    const int cellX = (baseCellX+offsetX+cellCount)%cellCount;
                    const int cellY = (baseCellY+offsetY+cellCount)%cellCount;
                    const std::uint32_t hash = texelHash(cellX,cellY,0x42414c4cU);
                    const int centreX = cellX*cellSize+static_cast<int>(hash%8U);
                    const int centreY = cellY*cellSize+static_cast<int>((hash>>8U)%8U);
                    const int dx = periodicDistance(x,centreX%size,size);
                    const int dy = periodicDistance(y,centreY%size,size);
                    // A slight per-stone anisotropy stretches cells so they are not all equant.
                    const int stretch = static_cast<int>((hash>>20U)%3U);
                    const int distanceSquared = stretch == 0 ? dx*dx*5/4+dy*dy
                                              : stretch == 1 ? dx*dx+dy*dy*5/4 : dx*dx+dy*dy;
                    if (distanceSquared < nearest)
                    {
                        second = nearest;
                        nearest = distanceSquared;
                        nearestHash = hash;
                        nearestDx = x-(centreX%size);
                        nearestDy = y-(centreY%size);
                    }
                    else if (distanceSquared < second) second = distanceSquared;
                }
            int value = 208+static_cast<int>((nearestHash>>16U)%37U)-18;
            // Facet lighting from the upper left; the gradient direction varies per stone.
            const int facet = static_cast<int>((nearestHash>>24U)%4U);
            const int lightX = facet < 2 ? -1 : 1;
            const int lightY = (facet&1) == 0 ? -1 : 1;
            value += std::clamp((nearestDx*lightX+nearestDy*lightY)*4,-22,22);
            if ((nearestHash>>12U)%9U == 0) value -= 34;
            if (second-nearest <= 3) value -= 58;
            else if (second-nearest <= 6) value -= 18;
            const std::uint32_t grit = texelHash(x,y,0x47524954U);
            value += static_cast<int>(grit%9U)-4;
            if ((grit&0xffU) < 5U) value -= 22;
            value = std::clamp(value,132,245);
            image.pixels[static_cast<std::size_t>(y*size+x)] =
                Color(value,value,value,255);
        }
    return image;
}

ProceduralImage makeCarpetDetailImage(int size)
{
    requireMaterialSize(size,"Carpet detail");
    ProceduralImage image{size,size,{}};
    image.pixels.resize(static_cast<std::size_t>(size*size));
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
        {
            const std::uint32_t fibre = texelHash(x,y,0x574f5645U);
            int value = 229+static_cast<int>(fibre%13U)-6;
            // An offset warp/weft pattern stays textile-like without turning into a high-contrast
            // checker when seen obliquely down the full six-car aisle.
            if ((x+(y/4)*2)%8 == 0) value -= 17;
            if ((y+(x/6))%12 == 0) value -= 9;
            if (((fibre>>8U)&0xffU) < 8U) value -= 20;
            value = std::clamp(value,178,247);
            image.pixels[static_cast<std::size_t>(y*size+x)] =
                Color(value,value,value,255);
        }
    return image;
}

ProceduralImage makeUpholsteryDetailImage(int size)
{
    requireMaterialSize(size,"Upholstery detail");
    // Filtering the native design, rather than undersampling its yarns, preserves
    // the same average colour and pattern at every supported lower resolution.
    if (size<256)
    {
        auto image=makeUpholsteryDetailImage(256);
        while (image.width>size) image=image.downsampled();
        return image;
    }
    ProceduralImage image{size,size,{}};
    image.pixels.resize(static_cast<std::size_t>(size*size));
    for (int y=0;y<size;++y)
        for (int x=0;x<size;++x)
        {
            // 16 cm tile: 0.625 mm texels, 1.25 mm warp/weft and larger broken
            // yarn flecks. Muted greys retain the six authored blue/teal palettes.
            // The flecks vary in angle, length and position, avoiding rows of dots.
            const int u=x*256/size, v=y*256/size;
            const bool warpOnTop=(u/2+v/2)%4<2;
            const int fibre=warpOnTop ? u%2 : v%2;
            const auto yarn=texelHash(u/2,v/2,0x5941524eU);
            int value=228+(fibre==0 ? 12 : -12)
                +static_cast<int>(yarn%25U)-12
                +static_cast<int>(texelHash(u,v,0x50494c45U)%9U)-4;
            const auto motif=texelHash(u/32,v/32,0x4d4f5155U);
            const float dx=static_cast<float>(u%32)-(11.0f+static_cast<float>(motif%11U));
            const float dy=static_cast<float>(v%32)-(11.0f+static_cast<float>((motif>>8U)%11U));
            const float lean=(motif&1U) ? 0.55f : -0.55f;
            const float along=dy+dx*lean;
            const float across=dx-dy*lean;
            const float length=7.0f+static_cast<float>((motif>>16U)%7U);
            const float envelope=std::clamp(1.0f-std::abs(along)/length,0.0f,1.0f);
            const float dark=std::clamp(4.2f-std::abs(across),0.0f,1.0f)*envelope;
            // A displaced pale yarn edge gives the mark a woven, broken contour;
            // it is baked albedo detail, not a simulated normal/specular response.
            const float pale=std::clamp(1.8f-std::abs(across-5.0f),0.0f,1.0f)*envelope;
            value+=static_cast<int>(-65.0f*dark+24.0f*pale);
            value=std::clamp(value,150,253);
            image.pixels[static_cast<std::size_t>(y*size+x)]=Color(value,value,value,255);
        }
    return image;
}

ProceduralImage makeConcreteDetailImage(int size)
{
    requireMaterialSize(size,"Concrete detail");
    ProceduralImage image{size,size,{}};
    image.pixels.resize(static_cast<std::size_t>(size*size));
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
        {
            const std::uint32_t grain = texelHash(x,y,0x434f4e43U);
            const std::uint32_t broad = texelHash(x/12,y/12,0x534c4142U);
            int value = 232+static_cast<int>(grain%11U)-5
                      + static_cast<int>(broad%13U)-6;
            if ((grain&0xffU) < 13U) value -= 24;
            if (((grain>>8U)&0xffU) > 246U) value += 10;
            value = std::clamp(value,192,249);
            image.pixels[static_cast<std::size_t>(y*size+x)] =
                Color(value,value,value,255);
        }
    return image;
}

ProceduralImage makeInteriorPanelDetailImage(int size)
{
    requireMaterialSize(size,"Interior panel detail");
    ProceduralImage image{size,size,{}};
    image.pixels.resize(static_cast<std::size_t>(size*size));
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
        {
            const std::uint32_t grain = texelHash(x,y,0x50414e4cU);
            const std::uint32_t broad = texelHash(x/16,y/16,0x4d454c41U);
            int value = 243+static_cast<int>(grain%7U)-3
                      + static_cast<int>(broad%5U)-2;
            // Very sparse inclusions and a low-contrast brushed direction stop the pale polymer
            // wall from becoming either television noise or a perfectly flat CG surface.
            if ((grain&0x3ffU) < 6U) value -= 18;
            if ((x+2*y)%31 == 0) value -= 3;
            value = std::clamp(value,216,250);
            image.pixels[static_cast<std::size_t>(y*size+x)] =
                Color(value,value,value,255);
        }
    return image;
}

ProceduralImage makeWaterDetailImage(int size)
{
    requireMaterialSize(size,"Water detail");
    ProceduralImage image{size,size,{}};
    image.pixels.resize(static_cast<std::size_t>(size*size));
    constexpr double tau = 6.2831853071795864769;
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
        {
            const double u = static_cast<double>(x)/static_cast<double>(size);
            const double v = static_cast<double>(y)/static_cast<double>(size);
            const std::uint32_t grain = texelHash(x,y,0x57415645U);
            const double broadWave = std::sin((u*4.0+v)*tau);
            const double crossWave = std::sin((u-v*2.0)*tau*2.0);
            int value = 232+static_cast<int>(std::lround(broadWave*8.0+crossWave*3.0))
                      + static_cast<int>(grain%5U)-2;
            if ((grain&0x1ffU) < 3U) value += 12;
            value = std::clamp(value,207,253);
            image.pixels[static_cast<std::size_t>(y*size+x)] =
                Color(value,value,value,255);
        }
    return image;
}

ProceduralImage makeResilientFloorDetailImage(int size)
{
    requireMaterialSize(size,"Resilient floor detail");
    ProceduralImage image{size,size,{}};
    image.pixels.resize(static_cast<std::size_t>(size*size));
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
        {
            const std::uint32_t grain = texelHash(x,y,0x52455349U);
            const std::uint32_t broad = texelHash(x/10,y/10,0x464c4f52U);
            int value = 235+static_cast<int>(grain%7U)-3
                      + static_cast<int>(broad%7U)-3;
            // Sparse light and dark chips resemble hard-wearing transport flooring while the
            // quiet background remains calm in a long oblique view through six vestibules.
            if ((grain&0xffU) < 8U) value -= 24;
            if (((grain>>8U)&0xffU) > 249U) value += 11;
            value = std::clamp(value,205,250);
            image.pixels[static_cast<std::size_t>(y*size+x)] =
                Color(value,value,value,255);
        }
    return image;
}

ProceduralImage makeMasonryDetailImage(int size)
{
    requireMaterialSize(size,"Masonry detail");
    ProceduralImage image{size,size,{}};
    image.pixels.resize(static_cast<std::size_t>(size*size));
    constexpr int courseHeight = 8;
    constexpr int brickWidth = 24;
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
        {
            const int course = y/courseHeight;
            const int offsetX = (x+(course%2)*brickWidth/2)%brickWidth;
            const bool horizontalMortar = y%courseHeight == 0;
            const bool verticalMortar = offsetX == 0;
            const std::uint32_t grain = texelHash(x,y,0x42524943U);
            int value = horizontalMortar || verticalMortar
                ? 201+static_cast<int>(grain%5U)
                : 236+static_cast<int>(grain%13U)-6;
            if (!horizontalMortar && !verticalMortar && (grain&0x1ffU) < 5U)
                value -= 15;
            value = std::clamp(value,195,246);
            image.pixels[static_cast<std::size_t>(y*size+x)] =
                Color(value,value,value,255);
        }
    return image;
}

ProceduralImage makeRenderedFacadeDetailImage(int size)
{
    requireMaterialSize(size,"Rendered facade detail");
    ProceduralImage image{size,size,{}};
    image.pixels.resize(static_cast<std::size_t>(size*size));
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
        {
            const std::uint32_t fine = texelHash(x,y,0x53545543U);
            const std::uint32_t medium = texelHash(x/7,y/7,0x504c4153U);
            const std::uint32_t broad = texelHash(x/23,y/23,0x54455245U);
            int value = 239+static_cast<int>(fine%7U)-3
                      + static_cast<int>(medium%9U)-4
                      + static_cast<int>(broad%13U)-6;
            // Rare shallow pits break an otherwise synthetic plaster plane. The broad component
            // supplies sun-readable trowel variation without baking a direction or colour into
            // the source, so the authored facade tint remains authoritative.
            if ((fine&0x3ffU) < 7U) value -= 18;
            value = std::clamp(value,207,250);
            image.pixels[static_cast<std::size_t>(y*size+x)] =
                Color(value,value,value,255);
        }
    return image;
}

std::unique_ptr<Texture2D> uploadMipmappedTexture(GraphicsDevice& device,
                                                  const ProceduralImage& image,int alphaTestReference)
{
    CnaEngine::Graphics::MipOptions options;
    if (alphaTestReference >= 0) options.alphaTestReference = alphaTestReference;
    return CnaEngine::Graphics::UploadTexture(
        device,engineImage(image,"Cannot upload an invalid procedural image"),options);
}

} // namespace TrainSim::Render
