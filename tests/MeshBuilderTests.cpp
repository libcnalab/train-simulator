#include "TestHarness.hpp"
#include "trainsim/render/WorldGeometry.hpp"
#include <future>
#include "trainsim/render/Daylight.hpp"
#include "trainsim/render/SkyGeometry.hpp"
#include "trainsim/render/AvatarProportions.hpp"
#include "trainsim/render/FacadeGeometry.hpp"
#include "trainsim/render/StreetPocketGeometry.hpp"
#include "trainsim/render/RoadVehicleGeometry.hpp"
#include "trainsim/render/GroundShadowGeometry.hpp"
#include "trainsim/render/StationGeometry.hpp"
#include "trainsim/render/ForecourtGeometry.hpp"
#include "trainsim/render/WaterSurface.hpp"
#include "trainsim/render/ShoreGeometry.hpp"
#include "trainsim/render/BridgeGeometry.hpp"
#include "trainsim/render/TunnelGeometry.hpp"
#include "trainsim/render/TrackGeometry.hpp"
#include "trainsim/render/LinesideGeometry.hpp"
#include "trainsim/render/SignalGeometry.hpp"
#include "trainsim/render/InteriorFittingsGeometry.hpp"
#include "trainsim/render/ToiletGeometry.hpp"
#include "trainsim/render/TrainExteriorGeometry.hpp"
#include "trainsim/render/TerminalGeometry.hpp"
#include "trainsim/render/OtherTrainGeometry.hpp"
#include "trainsim/render/Atmosphere.hpp"
#include "trainsim/render/NightLighting.hpp"
#include "trainsim/render/WeatherGeometry.hpp"
#include "trainsim/render/RoutePointCache.hpp"

#include "trainsim/render/MeshBuilder.hpp"
#include "trainsim/render/VegetationGeometry.hpp"
#include "trainsim/render/TerrainPatchGeometry.hpp"
#include "trainsim/render/VergeGeometry.hpp"
#include "trainsim/render/LandscapeVariation.hpp"
#include "trainsim/render/ViewCulling.hpp"
#include "trainsim/render/InteriorGeometry.hpp"
#include "trainsim/render/PassengerDisplayText.hpp"
#include "trainsim/render/PassengerDoorGeometry.hpp"
#include "trainsim/render/CarRoleGeometry.hpp"

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <tuple>

using Microsoft::Xna::Framework::Color;

TEST_CASE("water wave phases remain bounded and smooth after long service operation")
{
    using TrainSim::Render::waterWavePhases;
    for (double seconds : {0.0,-1.0,17.0,1e6,1e9,1e12})
    {
        const auto a=waterWavePhases(seconds);
        const auto b=waterWavePhases(seconds+0.016);
        for (std::size_t i=0;i<a.size();++i)
        {
            REQUIRE(std::isfinite(a[i]));
            REQUIRE(a[i]>=0.0f);
            REQUIRE(a[i]<=2.0*std::numbers::pi);
            const float delta=std::hypot(std::sin(b[i])-std::sin(a[i]),
                                         std::cos(b[i])-std::cos(a[i]));
            REQUIRE(delta>0.005f);
            REQUIRE(delta<0.020f);
        }
    }
    REQUIRE(waterWavePhases(std::numeric_limits<double>::infinity())==waterWavePhases(0.0));
    REQUIRE(waterWavePhases(std::numeric_limits<double>::quiet_NaN())==waterWavePhases(0.0));
}

TEST_CASE("shore contour tessellation preserves terrain area height UVs and winding")
{
    using namespace TrainSim::Render;
    using namespace Microsoft::Xna::Framework;
    const std::array<Vector3,4> corners={Vector3{0,-2,0},{0,1,50},{40,5,50},{40,2,0}};
    const std::array<Vector2,4> uvs={Vector2{0,0},{0,5},{4,5},{4,0}};
    MeshBuilder shore;
    addShoreTerrainCell(shore,corners,uvs,0,Color(90,130,70,255));
    const auto& data=shore.data();
    REQUIRE(data.indices.size()>6);
    REQUIRE(data.indices.size()<120);
    double area=0;
    for (const auto& v : data.vertices)
    {
        REQUIRE(std::abs(v.position.Y-(-2+v.position.X*0.1f+v.position.Z*0.06f))<1e-5f);
        REQUIRE(std::abs(v.textureCoordinate.X-v.position.X*0.1f)<1e-5f);
        REQUIRE(std::abs(v.textureCoordinate.Y-v.position.Z*0.1f)<1e-5f);
        REQUIRE(v.normal.Y>0.98f);
    }
    for (std::size_t i=0;i<data.indices.size();i+=3)
    {
        const auto a=data.vertices[data.indices[i]].position;
        const auto b=data.vertices[data.indices[i+1]].position;
        const auto c=data.vertices[data.indices[i+2]].position;
        const auto cross=Vector3::Cross(b-a,c-a);
        REQUIRE(cross.Y<0); // CNA clockwise front faces.
        area-=cross.Y*0.5;
    }
    REQUIRE(std::abs(area-2000)<0.01);
    auto flat=corners;
    for (auto& p : flat) p.Y=0.30f;
    MeshBuilder contour;
    addShoreTerrainCell(contour,flat,uvs,0,Color(90,130,70,255));
    REQUIRE(contour.data().indices.size()==6);
    const auto rising=shoreCoordinateAtHeight([](double x) { return static_cast<float>(x*0.5); },0,10,3);
    const auto falling=shoreCoordinateAtHeight([](double x) { return static_cast<float>(5-x*0.5); },0,10,3);
    REQUIRE(rising.has_value()); REQUIRE(falling.has_value());
    REQUIRE(std::abs(*rising-6)<0.001); REQUIRE(std::abs(*falling-4)<0.001);
    REQUIRE(!shoreCoordinateAtHeight([](double) { return 4.0f; },0,10,3));
    for (double d=80500;d<87200;d+=37)
    {
        REQUIRE(lakeShoreCoordinate(d,12)==12);
        REQUIRE(lakeShoreCoordinate(d,-500)==-500);
        REQUIRE(std::abs(lakeShoreCoordinate(d,480)-480)<=9);
        REQUIRE(std::abs(lakeShoreCoordinate(d+1,480)-lakeShoreCoordinate(d,480))<0.19);
        for (double side : {-1.0,1.0})
        {
            REQUIRE(riverShoreCoordinate(d,side*12)==12);
            REQUIRE(std::abs(riverShoreCoordinate(d,side*150)-150)<=9);
            REQUIRE(std::abs(riverShoreCoordinate(d+1,side*150)-riverShoreCoordinate(d,side*150))<0.19);
        }
    }
}

TEST_CASE("streaming background apron exceeds fog range without extending detail chunks")
{
    using TrainSim::Render::streamedBackdropRange;
    const auto first=streamedBackdropRange(8250,8500,286400,true,false);
    const auto last=streamedBackdropRange(9750,10000,286400,false,true);
    REQUIRE(first.first==7750); REQUIRE(first.second==8500);
    REQUIRE(last.first==9750); REQUIRE(last.second==10500);
    for (double train=9000;train<9250;train+=17)
    {
        REQUIRE(train-first.first>=1250);
        REQUIRE(last.second-train>1250);
    }
    const auto middle=streamedBackdropRange(8500,8750,286400,false,false);
    REQUIRE(middle.first==8500); REQUIRE(middle.second==8750);
    REQUIRE(streamedBackdropRange(0,250,286400,true,false).first==0);
    REQUIRE(streamedBackdropRange(286250,286400,286400,false,true).second==286400);
}

TEST_CASE("distant hill skirts are buried in sloping terrain without undersides")
{
    using namespace TrainSim::Render;
    using Microsoft::Xna::Framework::Vector3;
    for (int resolution : {4,8,12})
    {
        MeshBuilder builder;
        const auto ground=[](float along,float across) {
            return Vector3{across,along*0.03f+across*0.02f,along};
        };
        addGroundedHill(builder,ground,100,30,17,resolution,Color(100,130,120,255));
        const auto mesh=builder.take();
        REQUIRE(mesh.indices.size()==static_cast<std::size_t>(resolution*resolution*6));
        float maximum=0;
        for (const auto& vertex : mesh.vertices)
        {
            const auto p=vertex.position;
            const float relative=p.Y-ground(p.Z,p.X).Y;
            maximum=std::max(maximum,relative);
            REQUIRE(relative>=-0.301f);
            REQUIRE(relative<35.0f);
            REQUIRE(vertex.normal.Y>0.5f);
            REQUIRE(std::abs(vertex.normal.LengthSquared()-1)<1e-5f);
            if (std::abs(p.X)>99.99f || std::abs(p.Z)>111.99f)
                REQUIRE(std::abs(relative+0.30f)<1e-4f);
        }
        REQUIRE(maximum>15.0f);
    }
}
using Microsoft::Xna::Framework::Vector2;
using Microsoft::Xna::Framework::Vector3;
using TrainSim::Render::MeshBuilder;

TEST_CASE("printed station lettering is flat readable and inward facing on both banks")
{
    using namespace TrainSim::Render;
    using Microsoft::Xna::Framework::Matrix;
    for (float side : {-1.0f,1.0f})
    {
        MeshBuilder builder;
        addPrintedSignText(builder,"Pine Crossing",3.2f,
            Matrix::CreateRotationY(-side*1.570796327f)
                *Matrix::CreateTranslation(side*4.34f,2.30f,0),Color::White);
        const auto& mesh=builder.data();
        REQUIRE(mesh.indices.size()==12*6); // Space advances without drawing a block.
        for (const auto& v : mesh.vertices)
        {
            REQUIRE(std::abs(v.position.X-side*4.34f)<1e-5f);
            REQUIRE(std::abs(v.position.Z)<1.601f);
            REQUIRE(v.position.Y>2.0f && v.position.Y<2.6f);
            REQUIRE(v.normal.X*side< -0.999f);
            REQUIRE(v.textureCoordinate.X>=0 && v.textureCoordinate.X<=1);
            REQUIRE(v.textureCoordinate.Y>=0 && v.textureCoordinate.Y<=1);
        }
        REQUIRE(mesh.vertices.front().position.Z*side<mesh.vertices.back().position.Z*side);
    }
    MeshBuilder invalid;
    addPrintedSignText(invalid,"Test",0,Matrix::getIdentityProperty(),Color::White);
    REQUIRE(invalid.data().indices.empty());
}

TEST_CASE("station name boards have compact faces and platform-mounted supports")
{
    using namespace TrainSim::Render;
    using Microsoft::Xna::Framework::Matrix;
    MeshBuilder mesh;
    addStationNameBoard(mesh,Matrix::getIdentityProperty(),0.78f);
    REQUIRE(mesh.data().indices.size()==216);
    bool ground=false,top=false;
    for (const auto& v : mesh.data().vertices)
    {
        REQUIRE(std::abs(v.position.X)<=1.401f);
        REQUIRE(std::abs(v.position.Z)<=0.121f);
        REQUIRE(v.position.Y>=0.779f && v.position.Y<=2.691f);
        ground=ground || std::abs(v.position.Y-0.78f)<1e-5f;
        top=top || v.position.Y>2.68f;
    }
    REQUIRE(ground && top);
}

TEST_CASE("station facade families preserve the entrance while varying construction")
{
    using namespace TrainSim::Render;
    using Microsoft::Xna::Framework::Matrix;
    std::array<std::size_t,4> counts{};
    int index=0;
    for (auto family : {StationFacadeFamily::Rural,StationFacadeFamily::Urban,
                       StationFacadeFamily::Waterfront,StationFacadeFamily::Suburban})
    {
        MeshBuilder mesh;
        addStationFacadeIdentity(mesh,Matrix::getIdentityProperty(),16,4.8f,1.12f,family);
        counts[static_cast<std::size_t>(index++)]=mesh.data().indices.size();
        REQUIRE(mesh.data().indices.size()>100);
        REQUIRE(mesh.data().indices.size()<1200);
        for (const auto& v : mesh.data().vertices)
        {
            REQUIRE(std::abs(v.position.X)<=8.061f);
            REQUIRE(v.position.Y>=0.779f && v.position.Y<=4.801f);
            REQUIRE(v.position.Z>=-0.011f && v.position.Z<=0.761f);
            REQUIRE(std::abs(v.normal.LengthSquared()-1)<1e-4f);
            if (v.position.Y<3.30f)
                REQUIRE(std::abs(v.position.X-1.12f)>=0.849f);
        }
    }
    REQUIRE(counts[0]!=counts[1]);
    REQUIRE(counts[1]!=counts[3]);
    REQUIRE(!stationWindowClearsEntrance(2.0,0,1.85f));
    REQUIRE(stationWindowClearsEntrance(2.0,0,1.05f));
}

TEST_CASE("road car body leaves real wheel openings and bounded tapered extremities")
{
    using namespace TrainSim::Render;
    REQUIRE(roadCarArchBottom(0)==0.28f);
    for (float axle : {-1.3f,1.3f})
        REQUIRE(std::abs(roadCarArchBottom(axle)-0.66f)<1e-5f);
    MeshBuilder builder;
    addRoadCarBody(builder,Color(120,80,50,255));
    REQUIRE(builder.data().indices.size()<750);
    for (const auto& v : builder.data().vertices)
    {
        REQUIRE(std::abs(v.position.X)<=0.901f);
        REQUIRE(v.position.Y>=0.249f && v.position.Y<=1.021f);
        REQUIRE(std::abs(v.position.Z)<=2.051f);
        REQUIRE(std::abs(v.normal.LengthSquared()-1)<1e-4f);
        if (std::abs(v.position.X)>0.75f)
            REQUIRE(v.position.Y>=roadCarArchBottom(v.position.Z)-1e-5f);
    }
    for (float side : {-1.0f,1.0f})
    {
        MeshBuilder wheel;
        addRoadWheelHub(wheel,side,1.3f);
        REQUIRE(wheel.data().indices.size()==135);
        for (const auto& v : wheel.data().vertices)
        {
            REQUIRE(v.normal.X*side>0.95f);
            REQUIRE(v.position.X*side>=1.008f && v.position.X*side<=1.017f);
        }
    }
}

TEST_CASE("residential glazing keeps varied room cues inside the raised frame reveal")
{
    using namespace TrainSim::Render;
    using Microsoft::Xna::Framework::Matrix;
    for (unsigned variant=0;variant<4;++variant)
    {
        MeshBuilder builder,repeated;
        addResidentialGlazing(builder,1.2f,1.6f,Matrix::getIdentityProperty(),variant);
        addResidentialGlazing(repeated,1.2f,1.6f,Matrix::getIdentityProperty(),variant);
        const auto& mesh=builder.data();
        REQUIRE(mesh.indices.size()>=24);
        REQUIRE(mesh.indices.size()<=48);
        REQUIRE(mesh.indices==repeated.data().indices);
        bool front=false,back=false;
        std::map<std::uint32_t,int> colours;
        for (std::size_t i=0;i<mesh.vertices.size();++i)
        {
            const auto& v=mesh.vertices[i];
            REQUIRE(std::abs(v.position.X)<=0.6001f);
            REQUIRE(std::abs(v.position.Y)<=0.8001f);
            REQUIRE(std::abs(v.position.Z)>=0.0674f);
            REQUIRE(std::abs(v.position.Z)<=0.0691f);
            REQUIRE(std::abs(v.normal.Z)>0.999f);
            REQUIRE(v.color==repeated.data().vertices[i].color);
            REQUIRE(v.position==repeated.data().vertices[i].position);
            ++colours[v.color];
            front=front || v.normal.Z>0;
            back=back || v.normal.Z<0;
        }
        REQUIRE(front && back);
        REQUIRE(colours.size()>=4);
    }
}

TEST_CASE("residential entrance kits have bounded ground thresholds and two-sided hardware")
{
    using namespace TrainSim::Render;
    using Microsoft::Xna::Framework::Matrix;
    for (bool communal : {false,true})
    {
        MeshBuilder builder;
        addResidentialDoorKit(builder,1,2,Matrix::getIdentityProperty(),communal);
        const auto& mesh=builder.data();
        REQUIRE(mesh.indices.size()>=500);
        REQUIRE(mesh.indices.size()<=650);
        bool frontHandle=false,backHandle=false,header=false;
        for (const auto& v : mesh.vertices)
        {
            REQUIRE(std::abs(v.position.X)<=0.591f);
            REQUIRE(v.position.Y>=0 && v.position.Y<=2.081f);
            REQUIRE(std::abs(v.position.Z)<=0.241f);
            REQUIRE(std::abs(v.normal.LengthSquared()-1)<1e-5f);
            if (v.position.Y>0.9f && v.position.Y<1.0f)
            {
                frontHandle=frontHandle || v.position.Z>0.139f;
                backHandle=backHandle || v.position.Z< -0.139f;
            }
            header=header || v.position.Y>2.07f;
        }
        REQUIRE(frontHandle && backHandle && header);
    }
    MeshBuilder invalid;
    addResidentialDoorKit(invalid,0,2,Matrix::getIdentityProperty());
    REQUIRE(invalid.data().indices.empty());
}

TEST_CASE("street kerbs follow the road datum with upward chamfers and closed ends")
{
    MeshBuilder builder;
    const auto surface=[](float offset,double distance) {
        return Vector3{offset+static_cast<float>(distance*0.02),
            static_cast<float>(distance*0.03),static_cast<float>(distance)};
    };
    TrainSim::Render::addStreetKerb(builder,surface,0,25);
    const auto& mesh=builder.data();
    REQUIRE(mesh.indices.size()==54);
    bool bevel=false,top=false,front=false,back=false;
    for (const auto& v : mesh.vertices)
    {
        const float height=v.position.Y-v.position.Z*0.03f;
        REQUIRE(height>=-0.241f && height<=-0.069f);
        REQUIRE(std::abs(v.position.X-v.position.Z*0.02f)<=0.121f);
        REQUIRE(std::abs(v.normal.LengthSquared()-1)<1e-5f);
        bevel=bevel || (v.normal.Y>0.5f && v.normal.Y<0.9f);
        top=top || v.normal.Y>0.99f;
        front=front || v.normal.Z< -0.99f;
        back=back || v.normal.Z>0.99f;
    }
    REQUIRE(bevel && top && front && back);
    MeshBuilder invalid;
    TrainSim::Render::addStreetKerb(invalid,surface,2,1);
    REQUIRE(invalid.data().indices.empty());
}

TEST_CASE("access garden details preserve clear walking width on both street banks")
{
    for (float side : {-1.0f,1.0f})
    {
        MeshBuilder builder;
        const auto surface=[&](float fraction,float along) {
            return Vector3{fraction*10*side,fraction*0.4f,along};
        };
        TrainSim::Render::addAccessGarden(builder,surface,10,1.6f);
        REQUIRE(builder.data().indices.size()<1600);
        REQUIRE(builder.data().indices.size()>100);
        for (const auto& vertex : builder.data().vertices)
        {
            REQUIRE(std::abs(vertex.position.Z)>=0.799f);
            REQUIRE(std::abs(vertex.position.Z)<=1.821f);
            REQUIRE(vertex.position.X*side>=-0.001f && vertex.position.X*side<=10.001f);
            REQUIRE(std::isfinite(vertex.normal.Y));
        }
    }
}

TEST_CASE("passenger window frame closes pane gaps on both sides with bounded geometry")
{
    for (float side : {-1.0f,1.0f})
        for (float span : {0.081f,0.16f,0.7f,1.81f})
        {
            MeshBuilder mesh;
            TrainSim::Render::addPassengerWindowFrame(mesh,side,10,10+span);
            REQUIRE(mesh.data().indices.size()==600);
            REQUIRE(mesh.data().vertices.size()==400);
            for (const auto& v : mesh.data().vertices)
            {
                REQUIRE(std::isfinite(v.position.X));
                REQUIRE(v.position.X*side>=1.3959f && v.position.X*side<=1.5051f);
                REQUIRE(v.position.Y>=0.8449f && v.position.Y<=2.3551f);
                REQUIRE(v.position.Z>=9.9599f && v.position.Z<=10+span+0.0401f);
                REQUIRE(std::abs(v.normal.Length()-1)<0.0001f);
            }
            // Opaque interior flange overlaps both shell edges and adjacent mullions.
            float minY=3,maxY=0,minZ=20,maxZ=0;
            for (std::size_t i=0;i<80;++i)
            {
                const auto& p=mesh.data().vertices[i].position;
                minY=std::min(minY,p.Y); maxY=std::max(maxY,p.Y);
                minZ=std::min(minZ,p.Z); maxZ=std::max(maxZ,p.Z);
                REQUIRE(mesh.data().vertices[i].normal.X*side<0);
            }
            REQUIRE(minY<0.86f && maxY>2.34f);
            REQUIRE(minZ<9.97f && maxZ>10+span+0.03f);
            // No face is degenerate, including the short door-adjacent apertures.
            for (std::size_t i=0;i<mesh.data().indices.size();i+=3)
            {
                const auto a=mesh.data().vertices[mesh.data().indices[i]].position;
                const auto b=mesh.data().vertices[mesh.data().indices[i+1]].position;
                const auto c=mesh.data().vertices[mesh.data().indices[i+2]].position;
                REQUIRE(Vector3::Cross(b-a,c-a).LengthSquared()>1e-14f);
            }
        }
    MeshBuilder empty;
    TrainSim::Render::addPassengerWindowFrame(empty,1,1,1.07f);
    REQUIRE(empty.data().empty());
}

TEST_CASE("shaped cushion is closed bounded softly dished and consistently wound")
{
    MeshBuilder mesh;
    TrainSim::Render::addShapedSeatCushion(mesh,0,0,Color::White);
    REQUIRE(mesh.data().indices.size()==192*3);
    using Key=std::tuple<int,int,int>;
    const auto key=[](const Vector3& p) { return Key{
        static_cast<int>(std::lround(p.X*100000)),
        static_cast<int>(std::lround(p.Y*100000)),
        static_cast<int>(std::lround(p.Z*100000))}; };
    std::map<std::pair<Key,Key>,int> edges;
    bool hasDish=false,hasRim=false,hasRoll=false;
    for (const auto& v:mesh.data().vertices)
    {
        REQUIRE(std::abs(v.position.X)<=0.22501f);
        REQUIRE(std::abs(v.position.Z)<=0.35001f);
        REQUIRE(v.position.Y>=0.41999f && v.position.Y<=0.62001f);
        REQUIRE(std::abs(v.normal.Length()-1)<1e-5f);
        REQUIRE(std::isfinite(v.textureCoordinate.X) && std::isfinite(v.textureCoordinate.Y));
        hasDish |= std::abs(v.position.X)<1e-5f && std::abs(v.position.Z)<1e-5f
            && std::abs(v.position.Y-0.596f)<1e-5f;
        hasRim |= std::abs(v.position.Y-0.62f)<1e-5f;
        hasRoll |= v.normal.Y>0.70f && v.normal.Y<0.72f;
    }
    REQUIRE(hasDish && hasRim && hasRoll);
    for (std::size_t i=0;i<mesh.data().indices.size();i+=3)
    {
        const auto& a=mesh.data().vertices[mesh.data().indices[i]];
        const auto& b=mesh.data().vertices[mesh.data().indices[i+1]];
        const auto& c=mesh.data().vertices[mesh.data().indices[i+2]];
        const auto cross=Vector3::Cross(b.position-a.position,c.position-a.position);
        REQUIRE(cross.LengthSquared()>1e-12f);
        REQUIRE(Vector3::Dot(cross,a.normal+b.normal+c.normal)<0);
        const std::array<Key,3> keys={key(a.position),key(b.position),key(c.position)};
        for (int edge=0;edge<3;++edge)
        {
            auto lo=keys[edge],hi=keys[(edge+1)%3];
            if (hi<lo) std::swap(lo,hi);
            ++edges[{lo,hi}];
        }
    }
    for (const auto& [edge,count]:edges) { (void)edge; REQUIRE(count==2); }
}

TEST_CASE("seat upholstery retains backrest geometry and maps fabric in seat local coordinates")
{
    for (bool forward : {false,true})
    {
        const float backZ=forward ? 0.28f : -0.38f;
        MeshBuilder mesh,legacy,translated;
        TrainSim::Render::addSeatUpholsteryBody(mesh,0,0,backZ,Color::White);
        TrainSim::Render::addSeatUpholsteryBody(translated,12,60,60+backZ,Color::White);
        TrainSim::Render::addShapedSeatCushion(legacy,0,0,Color::White);
        legacy.addRoundedBoxXY({-0.225f,0.58f,backZ},{0.225f,1.56f,backZ+0.10f},
                               0.055f,2,Color::White);
        legacy.addEllipsoid({0,1.505f,backZ+0.05f},{0.222f,0.105f,0.082f},
                            24,10,Color::White,true);
        REQUIRE(mesh.data().indices==legacy.data().indices);
        REQUIRE(mesh.data().vertices.size()==legacy.data().vertices.size());
        REQUIRE(mesh.data().indices==translated.data().indices);
        int mappedSides=0,mappedTop=0;
        for (std::size_t i=0;i<mesh.data().vertices.size();++i)
        {
            const auto& v=mesh.data().vertices[i];
            REQUIRE((v.position-legacy.data().vertices[i].position).LengthSquared()<1e-12f);
            REQUIRE((v.normal-legacy.data().vertices[i].normal).LengthSquared()<1e-12f);
            const auto uv=v.textureCoordinate;
            const auto moved=translated.data().vertices[i].textureCoordinate;
            REQUIRE(std::isfinite(uv.X) && std::isfinite(uv.Y));
            REQUIRE(std::abs(uv.X-moved.X)<0.0001f);
            REQUIRE(std::abs(uv.Y-moved.Y)<0.0001f);
            if (v.normal.Y>0.999f && v.position.Y<0.63f)
            {
                REQUIRE(std::abs(uv.X-(v.position.X+0.225f)/0.16f)<1e-5f);
                REQUIRE(std::abs(uv.Y-(v.position.Z+0.35f)/0.16f)<1e-5f);
                ++mappedTop;
            }
            if (std::abs(v.normal.Z)>0.999f && std::abs(std::abs(v.position.Z)-0.35f)<1e-5f)
            {
                const float unfolded=0.35f+0.027f+0.57f-v.position.Y;
                const float expected=(0.35f+(v.position.Z>0 ? unfolded : -unfolded))/0.16f;
                REQUIRE(std::abs(uv.Y-expected)<1e-5f);
                ++mappedSides;
            }
        }
        REQUIRE(mappedTop>0 && mappedSides>0);
    }
}

TEST_CASE("canopy shade projects sunlight to receiver height rather than directly below roof")
{
    using TrainSim::Render::canopyShadowBounds;
    using TrainSim::Render::canopyShadeAt;
    const auto floor=canopyShadowBounds(40,0.78f,{0.5f,-1,0.25f});
    REQUIRE(std::abs(floor.minX-(-2.30f+1.455f)) < 1e-5f);
    REQUIRE(std::abs(floor.maxZ-(20+0.7275f)) < 1e-5f);
    REQUIRE(canopyShadeAt(floor,1,0) == 1);
    REQUIRE(canopyShadeAt(floor,-2,0) == 0);
    REQUIRE(std::abs(canopyShadeAt(floor,floor.minX+0.125f,0)-0.5f) < 1e-5f);
    const auto torso=canopyShadowBounds(40,1.83f,{0.5f,-1,0.25f});
    REQUIRE(torso.minX < floor.minX);
    const auto opposite=canopyShadowBounds(40,0.78f,{-0.5f,-1,-0.25f});
    REQUIRE(std::abs(opposite.minX+floor.maxX) < 1e-5f);
    REQUIRE(std::abs(opposite.maxZ+floor.minZ) < 1e-5f);
    REQUIRE(canopyShadeAt(canopyShadowBounds(40,4,{0,-1,0}),0,0) == 0);
    REQUIRE(canopyShadeAt(canopyShadowBounds(40,0,{1,0,0}),0,0) == 0);
}

TEST_CASE("streaming mesh clear retains staging capacity without retaining stale geometry")
{
    MeshBuilder mesh;
    mesh.reserve(1000,1500);
    mesh.addBox({0,0,0},{1,1,1},Color::White);
    const auto vertices=mesh.data().vertices.capacity(),indices=mesh.data().indices.capacity();
    mesh.clear();
    REQUIRE(mesh.data().vertices.empty());
    REQUIRE(mesh.data().indices.empty());
    REQUIRE(mesh.data().vertices.capacity()==vertices);
    REQUIRE(mesh.data().indices.capacity()==indices);
    mesh.addTriangle({10,0,0},{10,0,1},{11,0,1},Color::Green);
    REQUIRE(mesh.data().vertices.size()==3);
    REQUIRE(mesh.data().indices.size()==3);
    REQUIRE(mesh.data().vertices.front().position.X==10);
    for (auto index : mesh.data().indices) REQUIRE(index<3);
}

TEST_CASE("crop patches follow both terrain triangles without bridging the cell diagonal")
{
    const std::array<Vector3,4> corners={Vector3{0,0,0},{0,0,10},{10,6,10},{10,0,0}};
    MeshBuilder mesh;
    TrainSim::Render::addTerrainCellPatch(mesh,corners,0.2f,0.9f,0.1f,0.7f,Color::White);
    REQUIRE(mesh.data().indices.size()==12);
    float area=0;
    for (std::size_t i=0;i<mesh.data().indices.size();i+=3)
    {
        const auto a=mesh.data().vertices[mesh.data().indices[i]].position;
        const auto b=mesh.data().vertices[mesh.data().indices[i+1]].position;
        const auto c=mesh.data().vertices[mesh.data().indices[i+2]].position;
        const auto centre=(a+b+c)/3;
        REQUIRE(std::abs(centre.Y-(0.6f*std::min(centre.X,centre.Z)+0.018f))<0.00001f);
        area+=std::abs(Vector3::Cross(b-a,c-a).Y)*0.5f;
    }
    REQUIRE(std::abs(area-42)<0.0001f);
    for (const auto& vertex : mesh.data().vertices)
    {
        REQUIRE(vertex.normal.Y>0);
        REQUIRE(vertex.position.X>=0.9999f && vertex.position.X<=7.0001f);
        REQUIRE(vertex.position.Z>=1.9999f && vertex.position.Z<=9.0001f);
    }
    MeshBuilder empty;
    TrainSim::Render::addTerrainCellPatch(empty,corners,1,0,0,1,Color::White);
    REQUIRE(empty.data().empty());
}

TEST_CASE("tree trunks scale in every axis and close both joints and end caps")
{
    using TrainSim::Render::addTaperedTreeTrunk;
    for (int sides : {5,6,7,9})
    {
        MeshBuilder reference;
        addTaperedTreeTrunk(reference,{},1,3.8f,sides,Color::White);
        REQUIRE(reference.data().indices.size()/3==static_cast<std::size_t>(6*sides));
        using Key=std::tuple<int,int,int>;
        const auto key=[](const Vector3& p) { return Key{
            static_cast<int>(std::lround(p.X*100000)),static_cast<int>(std::lround(p.Y*100000)),
            static_cast<int>(std::lround(p.Z*100000))}; };
        std::map<std::pair<Key,Key>,int> edges;
        const auto& geometry=reference.data();
        for (std::size_t i=0;i<geometry.indices.size();i+=3)
            for (int edge=0;edge<3;++edge)
            {
                auto lo=key(geometry.vertices[geometry.indices[i+edge]].position);
                auto hi=key(geometry.vertices[geometry.indices[i+(edge+1)%3]].position);
                if (hi<lo) std::swap(lo,hi);
                ++edges[{lo,hi}];
            }
        for (const auto& [edge,count] : edges) { (void)edge; REQUIRE(count==2); }
        for (float scale : {0.25f,0.46f,0.9f,1.45f,2.5f,4.0f})
        {
            const Vector3 root{11,-3,17};
            MeshBuilder actual;
            addTaperedTreeTrunk(actual,root,scale,3.8f,sides,Color::White);
            REQUIRE(actual.data().vertices.size()==reference.data().vertices.size());
            float top=root.Y;
            for (std::size_t i=0;i<actual.data().vertices.size();++i)
            {
                const auto& v=actual.data().vertices[i];
                REQUIRE((v.position-root-reference.data().vertices[i].position*scale).Length()<0.00001f);
                REQUIRE(std::abs(v.normal.Length()-1)<0.001f);
                top=std::max(top,v.position.Y);
            }
            REQUIRE(std::abs(top-root.Y-3.8f*scale)<0.00001f);
            // Highest production branch root is 2.51 authored metres; it remains inside the trunk.
            REQUIRE(top>root.Y+2.51f*scale);
        }
    }
    MeshBuilder empty;
    addTaperedTreeTrunk(empty,{},0,3.8f,9,Color::White);
    REQUIRE(empty.data().vertices.empty());
}

TEST_CASE("tree identities vary by route position and remain stable inside coordinate bins")
{
    using TrainSim::Render::treeShapeSeed;
    const auto seed=treeShapeSeed(52000,40,3);
    REQUIRE(seed==treeShapeSeed(52000.001,40.001,3));
    REQUIRE(seed!=treeShapeSeed(52001,40,3));
    REQUIRE(seed!=treeShapeSeed(52000,-40,3));
    REQUIRE(seed!=treeShapeSeed(52000,40,0));
    MeshBuilder a,b;
    TrainSim::Render::addConiferCrown(a,{},1,9,seed,Color(40,82,43),Color(61,105,54),true);
    TrainSim::Render::addConiferCrown(b,{},1,9,treeShapeSeed(52001,40,3),
        Color(40,82,43),Color(61,105,54),true);
    REQUIRE(a.data().indices==b.data().indices);
    int changed=0;
    for (std::size_t i=0;i<a.data().vertices.size();++i)
        if ((a.data().vertices[i].position-b.data().vertices[i].position).LengthSquared()>0.0001f) ++changed;
    REQUIRE(changed>100);
}

TEST_CASE("conifer whorls are bounded deterministic and independent of world origin")
{
    using TrainSim::Render::addConiferCrown;
    for (bool close : {false,true})
    {
        MeshBuilder first, shifted, repeated;
        const Vector3 origin{128,4,-256};
        REQUIRE(addConiferCrown(first,{},1,9,3,Color(40,82,43),Color(61,105,54),close)
                == (close ? 7 : 3));
        addConiferCrown(shifted,origin,1,9,3,Color(40,82,43),Color(61,105,54),close);
        addConiferCrown(repeated,{},1,9,3,Color(40,82,43),Color(61,105,54),close);
        const auto& a=first.data();
        const auto& b=shifted.data();
        REQUIRE(a.indices.size() == (close ? 1512U : 162U));
        REQUIRE(a.vertices.size() == b.vertices.size());
        REQUIRE(a.indices == repeated.data().indices);
        for (std::size_t i=0;i<a.vertices.size();++i)
        {
            const auto& v=a.vertices[i];
            REQUIRE(std::abs(v.position.X) < 2.12f);
            REQUIRE(std::abs(v.position.Z) < 2.12f);
            REQUIRE(v.position.Y > 1.3f);
            REQUIRE(v.position.Y < 7.0f);
            REQUIRE((b.vertices[i].position-origin-v.position).LengthSquared() < 1e-8f);
            REQUIRE(std::abs(v.normal.LengthSquared()-1.0f) < 0.001f);
            REQUIRE(v.color == b.vertices[i].color);
            REQUIRE(std::abs(v.textureCoordinate.X-b.vertices[i].textureCoordinate.X)<0.0001f);
            REQUIRE(std::abs(v.textureCoordinate.Y-b.vertices[i].textureCoordinate.Y)<0.0001f);
            REQUIRE(v.position == repeated.data().vertices[i].position);
        }
    }
    MeshBuilder invalid;
    REQUIRE(addConiferCrown(invalid,{},0,9,0,Color(),Color(),true) == 0);
    REQUIRE(invalid.data().vertices.empty());
}

TEST_CASE("conifer cutout fronds replace broad solid fans within a bounded shared batch")
{
    for (unsigned seed : {0U,1U,96U,0xffffffffU})
    {
        MeshBuilder core,sprays;
        TrainSim::Render::addConiferCrown(core,{},1,9,seed,Color::Green,Color::White,true,&sprays);
        REQUIRE(core.data().indices.size()==7*6*9*3);
        REQUIRE(sprays.data().indices.size()==7*12*9*3);
        for (const auto& v : sprays.data().vertices)
        {
            REQUIRE(std::abs(v.position.X)<2.2f && std::abs(v.position.Z)<2.2f);
            REQUIRE(v.position.Y>0.95f && v.position.Y<7);
            REQUIRE(std::abs(v.normal.Length()-1)<0.001f);
            REQUIRE(v.textureCoordinate.X>=0 && v.textureCoordinate.X<=1);
            REQUIRE(v.textureCoordinate.Y>=0 && v.textureCoordinate.Y<=1);
        }
    }
}

TEST_CASE("conifer branch fans keep a bounded budget across detail and seed variants")
{
    for (int sides : {3,6,9,12,18})
        for (unsigned seed : {0U,1U,32U,96U,0x9e3779b9U,0xffffffffU})
        {
            MeshBuilder mesh;
            TrainSim::Render::addConiferCrown(mesh,{},1.18f,sides,seed,
                Color(40,82,43),Color(61,105,54),true);
            REQUIRE(mesh.data().indices.size()==static_cast<std::size_t>(7*8*std::clamp(sides,6,12)*3));
            bool varyingUV=false;
            for (const auto& vertex : mesh.data().vertices)
            {
                REQUIRE(std::abs(vertex.position.X)<2.12f*1.18f);
                REQUIRE(std::abs(vertex.position.Z)<2.12f*1.18f);
                REQUIRE(vertex.position.Y>1.3f*1.18f);
                REQUIRE(vertex.position.Y<7.0f*1.18f);
                REQUIRE(std::abs(vertex.normal.LengthSquared()-1)<0.001f);
                varyingUV=varyingUV || std::abs(vertex.textureCoordinate.X)>0.1f;
            }
            REQUIRE(varyingUV);
        }
}

TEST_CASE("bounded route point cache preserves exact geometry across hits and evictions")
{
    const auto route=TrainSim::Sim::Route::NorthernValleyExpress();
    TrainSim::Render::RoutePointCache cache;
    for (double reference : {-200.0,0.0,52000.0,286400.0,286600.0})
        for (int sample=0;sample<300;++sample)
            for (double lateral : {-100.0,0.0,12.0,75.0})
            {
                const double distance=reference+sample*7.31-500;
                const auto pose=route.relativePose(reference,distance);
                const auto point=cache.point(route,reference,distance,lateral);
                REQUIRE(point.x == static_cast<float>(pose.x+lateral*std::cos(pose.headingRadians)));
                REQUIRE(point.y == static_cast<float>(pose.elevation));
                REQUIRE(point.z == static_cast<float>(pose.z-lateral*std::sin(pose.headingRadians)));
                REQUIRE(point.yaw == static_cast<float>(pose.headingRadians));
            }
    REQUIRE(cache.misses() == 1500);
    REQUIRE(cache.hits() == 4500);
}

TEST_CASE("platform soffit faces down and leaves room for the central lights")
{
    MeshBuilder builder;
    TrainSim::Render::addCanopySoffit(builder,Vector3::Zero,30,0);
    const auto mesh=builder.take();
    REQUIRE(mesh.indices.size() == (20*12+24)*3);
    for (std::size_t i=0;i<mesh.vertices.size();++i)
    {
        const auto& v=mesh.vertices[i];
        REQUIRE(std::abs(v.position.X) <= 2.19f);
        REQUIRE(std::abs(v.position.Z) <= 15);
        REQUIRE(v.position.Y >= 3.369f && v.position.Y < 3.51f);
        if (i < 480) REQUIRE(v.normal.Y < -0.99f);
        else REQUIRE(std::abs(v.position.X) > 1.60f);
    }
    TrainSim::Render::addCanopySoffit(builder,Vector3::Zero,0,0);
    REQUIRE(builder.take().empty());
}

TEST_CASE("canopy frames join the roof rails and remain grounded on the platform")
{
    using namespace TrainSim::Render;
    using Microsoft::Xna::Framework::Matrix;
    for (float postX : {-0.2f,0.2f})
    {
        MeshBuilder frame;
        addCanopyFrame(frame,Matrix::getIdentityProperty(),postX,0.78f);
        REQUIRE(frame.data().indices.size()==360);
        bool grounded=false,head=false,top=false;
        for (const auto& v : frame.data().vertices)
        {
            REQUIRE(std::abs(v.position.X)<=2.211f);
            REQUIRE(std::abs(v.position.Z)<=0.171f);
            REQUIRE(v.position.Y>=0.779f && v.position.Y<=3.451f);
            REQUIRE(std::abs(v.normal.LengthSquared()-1)<1e-5f);
            grounded=grounded || std::abs(v.position.Y-0.78f)<1e-5f;
            head=head || std::abs(v.position.Y-3.20f)<1e-5f;
            top=top || v.position.Y>3.44f;
        }
        REQUIRE(grounded && head && top);
    }
    MeshBuilder structure,finish;
    addCanopySoffit(structure,Vector3::Zero,30,0,&finish);
    REQUIRE(structure.data().indices.size()==72);
    REQUIRE(finish.data().indices.size()==720);
    for (const auto& v : finish.data().vertices) REQUIRE(v.normal.Y< -0.99f);
}

TEST_CASE("platform paving has bounded staggered modules and upward bevels on both banks")
{
    using namespace TrainSim::Render;
    using Microsoft::Xna::Framework::Matrix;
    for (float side : {-1.0f,1.0f})
    {
        MeshBuilder paving;
        const int slabs=addPlatformPaving(paving,Matrix::getIdentityProperty(),180,side,Color(180,180,180,255));
        REQUIRE(slabs>2000 && slabs<2100);
        REQUIRE(paving.data().indices.size()==static_cast<std::size_t>(slabs)*30);
        for (const auto& v : paving.data().vertices)
        {
            REQUIRE(v.position.X*side>=-1.798f && v.position.X*side<=2.399f);
            REQUIRE(std::abs(v.position.Z)<89);
            REQUIRE(v.position.Y>=0.0029f && v.position.Y<=0.0081f);
            REQUIRE(v.normal.Y>0.5f);
            REQUIRE(std::abs(v.normal.LengthSquared()-1)<1e-5f);
        }
    }
    MeshBuilder invalid;
    REQUIRE(addPlatformPaving(invalid,Matrix::getIdentityProperty(),0,1,Color::White)==0);
    REQUIRE(addPlatformPaving(invalid,Matrix::getIdentityProperty(),180,0,Color::White)==0);
    REQUIRE(invalid.data().empty());
}

TEST_CASE("platform tactile modules have millimetre relief and a bounded close-range population")
{
    using namespace TrainSim::Render;
    using Microsoft::Xna::Framework::Matrix;
    MeshBuilder relief,tiles;
    const int blisters=addPlatformTactile(relief,tiles,Matrix::getIdentityProperty(),180);
    REQUIRE(blisters>13000 && blisters<=14100);
    REQUIRE(relief.data().indices.size()==static_cast<std::size_t>(blisters)*18);
    REQUIRE(tiles.data().indices.size()>=444*6 && tiles.data().indices.size()<=445*6);
    for (const auto& v : relief.data().vertices)
    {
        REQUIRE(std::abs(v.position.X)<0.19f && std::abs(v.position.Z)<78.21f);
        REQUIRE(v.position.Y>=0.0099f && v.position.Y<=0.0151f);
        REQUIRE(v.normal.Y>0.85f);
        REQUIRE(std::abs(v.normal.LengthSquared()-1)<1e-5f);
    }
    for (const auto& v : tiles.data().vertices) REQUIRE(v.normal.Y>0.99f);
    MeshBuilder invalid;
    REQUIRE(addPlatformTactile(invalid,invalid,Matrix::getIdentityProperty(),0)==0);
    REQUIRE(invalid.data().empty());
}

TEST_CASE("terminal forecourt is bounded, terrain following and upward on either endpoint")
{
    using namespace TrainSim;
    using namespace TrainSim::Render;
    for (float direction : {-1.0f,1.0f})
        for (float side : {-1.0f,1.0f})
        {
            MeshBuilder structure,paving;
            const auto ground=[&](float along,float across) {
                return Vector3{across*side,along*0.002f+World::forecourtHeightAboveGround(across),along*direction};
            };
            const int slabs=addTerminalForecourt(structure,paving,ground);
            REQUIRE(slabs>2400 && slabs<2900);
            REQUIRE(paving.data().indices.size()<36000);
            for (const auto& v : paving.data().vertices)
            {
                REQUIRE(v.position.X*side>=6.699f && v.position.X*side<=51.901f);
                REQUIRE(v.position.Z*direction>=-90.001f && v.position.Z*direction<=47.001f);
                REQUIRE(v.normal.Y>0.99f);
                REQUIRE(std::abs(v.normal.LengthSquared()-1)<1e-5f);
            }
        }
    REQUIRE(std::abs(World::forecourtHeightAboveGround(World::ForecourtInner)-0.78f)<1e-5f);
    REQUIRE(std::abs(World::forecourtHeightAboveGround(World::ForecourtOuter)-0.16f)<1e-5f);
}

TEST_CASE("market clock turret face sits outside its wall and keeps a bounded architectural kit")
{
    using namespace TrainSim::Render;
    MeshBuilder face;
    addMarketTurretFace(face,Microsoft::Xna::Framework::Matrix::getIdentityProperty());
    REQUIRE(face.data().indices.size()>800*3 && face.data().indices.size()<900*3);
    bool clock=false;
    for (const auto& v : face.data().vertices)
    {
        REQUIRE(std::abs(v.position.X)<1.47f);
        REQUIRE(v.position.Y>2.3f && v.position.Y<9.22f);
        REQUIRE(v.position.Z>=2.199f && v.position.Z<=2.451f);
        REQUIRE(std::abs(v.normal.LengthSquared()-1)<1e-5f);
        if (std::abs(v.position.Z-marketTowerClockFaceDepth)<1e-5f && v.position.Y>7.4f)
            clock=true;
    }
    REQUIRE(clock);
    REQUIRE(hasMarketTownTurret(1) && hasMarketTownTurret(6));
    REQUIRE(!hasMarketTownTurret(0) && !hasMarketTownTurret(11));
}

TEST_CASE("station wall finishes wrap corners without blocking the public entrance")
{
    using namespace TrainSim::Render;
    using Microsoft::Xna::Framework::Matrix;
    REQUIRE(stationMaterialPalette(StationFacadeFamily::Urban,false).brickUpper);
    REQUIRE(!stationMaterialPalette(StationFacadeFamily::Urban,true).brickUpper);
    REQUIRE(!stationMaterialPalette(StationFacadeFamily::Rural,false).brickUpper);
    for (float side : {-1.0f,1.0f})
    {
        MeshBuilder finish;
        addStationWallFinish(finish,Matrix::getIdentityProperty(),16,42,9.5f,1.25f,2.94f,side,
            Color(180,180,170,255),true);
        REQUIRE(finish.data().indices.size()>2000 && finish.data().indices.size()<8000);
        for (const auto& v : finish.data().vertices)
        {
            REQUIRE(std::abs(v.position.X)<=8.076f && std::abs(v.position.Z)<=21.066f);
            REQUIRE(v.position.Y>=1.249f && v.position.Y<9.5f);
            REQUIRE(std::abs(v.normal.LengthSquared()-1)<1e-5f);
            if (v.position.X*side< -7.99f && v.position.Y<1.4f)
                REQUIRE(std::abs(v.position.Z-2.94f)>=0.879f);
        }
    }
}

TEST_CASE("station glazing keeps varied room cues inset behind both window frames")
{
    using namespace TrainSim::Render;
    using Microsoft::Xna::Framework::Matrix;
    for (unsigned seed=0;seed<8;++seed)
    {
        MeshBuilder pane;
        addStationGlazing(pane,1.7f,1.45f,Matrix::getIdentityProperty(),seed);
        REQUIRE(pane.data().indices.size()>=72 && pane.data().indices.size()<=120);
        bool positive=false,negative=false;
        for (const auto& v : pane.data().vertices)
        {
            REQUIRE(std::abs(v.position.X)<=0.8501f && std::abs(v.position.Y)<=0.7251f);
            REQUIRE(std::abs(v.position.Z)>=0.0674f && std::abs(v.position.Z)<0.07f);
            REQUIRE(v.normal.Z*v.position.Z>0.067f);
            positive=positive || v.normal.Z>0.99f;
            negative=negative || v.normal.Z< -0.99f;
        }
        REQUIRE(positive && negative);
    }
    MeshBuilder invalid;
    addStationGlazing(invalid,0,1,Matrix::getIdentityProperty(),0);
    REQUIRE(invalid.data().empty());
}

TEST_CASE("departure display has a closed chamfered case and grounded supports below the canopy")
{
    using namespace TrainSim::Render;
    MeshBuilder board;
    addPlatformDepartureBoard(board,Microsoft::Xna::Framework::Matrix::getIdentityProperty(),0.78f);
    REQUIRE(board.data().indices.size()==264*3);
    bool screen=false,foot=false;
    for (const auto& v : board.data().vertices)
    {
        REQUIRE(std::abs(v.position.X)<=1.586f);
        REQUIRE(v.position.Y>=0.779f && v.position.Y<3.20f);
        REQUIRE(v.position.Z>=-0.141f && v.position.Z<=0.121f);
        REQUIRE(std::abs(v.normal.LengthSquared()-1)<1e-5f);
        if (v.position.Z>0.10f && v.position.Y>1.9f) REQUIRE(std::abs(v.position.X)>1.54f);
        screen=screen || (std::abs(v.position.Z-0.095f)<1e-5f && v.normal.Z>0.99f);
        foot=foot || v.position.Y<0.781f;
    }
    REQUIRE(screen && foot);
}

TEST_CASE("platform fence end posts are capped and grounded above the paving")
{
    using namespace TrainSim::Render;
    MeshBuilder post;
    addPlatformFencePost(post,Microsoft::Xna::Framework::Matrix::getIdentityProperty());
    REQUIRE(post.data().indices.size()==36*3);
    bool foot=false,cap=false;
    for (const auto& v : post.data().vertices)
    {
        REQUIRE(std::abs(v.position.X)<=0.091f && std::abs(v.position.Z)<=0.091f);
        REQUIRE(v.position.Y>=0.0079f && v.position.Y<=1.326f);
        REQUIRE(std::abs(v.normal.LengthSquared()-1)<1e-5f);
        foot=foot || v.position.Y<0.009f;
        cap=cap || v.position.Y>1.32f;
    }
    REQUIRE(foot && cap);
}

TEST_CASE("platform bin has an open recessed mouth and a bounded finished shell")
{
    using namespace TrainSim::Render;
    using Microsoft::Xna::Framework::Matrix;
    MeshBuilder bin;
    addPlatformBin(bin,Matrix::getIdentityProperty());
    REQUIRE(bin.data().indices.size()>250*3 && bin.data().indices.size()<350*3);
    bool liner=false,lip=false,grounded=false;
    for (const auto& v : bin.data().vertices)
    {
        REQUIRE(std::abs(v.position.X)<=0.291f && std::abs(v.position.Z)<=0.301f);
        REQUIRE(v.position.Y>=0.0079f && v.position.Y<=0.961f);
        REQUIRE(std::abs(v.normal.LengthSquared()-1)<1e-5f);
        if (v.position.Z>0.27f && v.position.Y>0.65f && v.position.Y<0.81f)
            REQUIRE(std::abs(v.position.X)>0.20f);
        liner=liner || (std::abs(v.position.Z-0.04f)<1e-5f && v.position.Y>0.8f);
        lip=lip || (v.position.Z>0.27f && std::abs(v.position.Y-0.648f)<1e-5f);
        grounded=grounded || v.position.Y<0.009f;
    }
    REQUIRE(liner && lip && grounded);
}

TEST_CASE("canopy luminaires separate a downward diffuser from a grounded sealed housing")
{
    using namespace TrainSim::Render;
    using Microsoft::Xna::Framework::Matrix;
    MeshBuilder housing,emitter;
    addCanopyLuminaire(housing,emitter,Matrix::getIdentityProperty());
    REQUIRE(housing.data().indices.size()==124*3);
    REQUIRE(emitter.data().indices.size()==32*3);
    bool mounting=false,downward=false;
    for (const auto& v : housing.data().vertices)
    {
        REQUIRE(std::abs(v.position.X)<=0.776f && std::abs(v.position.Z)<=0.171f);
        REQUIRE(v.position.Y>=3.349f && v.position.Y<=3.490f);
        REQUIRE(std::abs(v.normal.LengthSquared()-1)<1e-5f);
        mounting=mounting || v.position.Y>3.487f;
    }
    for (const auto& v : emitter.data().vertices)
    {
        REQUIRE(std::abs(v.position.X)<=0.691f && std::abs(v.position.Z)<=0.101f);
        REQUIRE(v.position.Y>=3.324f && v.position.Y<=3.356f);
        REQUIRE(std::abs(v.normal.LengthSquared()-1)<1e-5f);
        downward=downward || v.normal.Y< -0.99f;
    }
    REQUIRE(mounting && downward);
}

TEST_CASE("terminal skyline facades face outward and stay inside building height")
{
    for (float side : {-1.0f,1.0f})
    {
        MeshBuilder builder;
        TrainSim::Render::addTerminalSkylineFacade(builder,Vector3::Zero,18,8,30,0,side);
        const auto mesh=builder.take();
        REQUIRE(mesh.indices.size() == (7+4+4)*2*4*3);
        for (const auto& v : mesh.vertices)
        {
            REQUIRE(v.position.Y >= -0.071f && v.position.Y <= 8);
            REQUIRE(std::abs(v.position.X) <= 9.046f);
            REQUIRE(std::abs(v.position.Z) <= 15.046f);
            if (std::abs(v.position.X) > 9)
                REQUIRE(v.normal.X*side < -0.99f);
            else REQUIRE(v.normal.Z*v.position.Z > 14.9f);
        }
    }
}

TEST_CASE("terminal entrance terraces are mirrored bounded and leave their central access clear")
{
    for (float side : {-1.0f,1.0f})
    {
        MeshBuilder builder;
        TrainSim::Render::addTerminalFrontage(builder,{},18,30,0,side);
        const auto& mesh=builder.data();
        REQUIRE(!mesh.empty());
        REQUIRE(mesh.indices.size() < 1500);
        for (const auto& v : mesh.vertices)
        {
            const float inward=-side*v.position.X-9;
            REQUIRE(inward >= -0.081f && inward <= 6.001f);
            REQUIRE(std::abs(v.position.Z) <= 17.001f);
            REQUIRE(v.position.Y >= -1.501f && v.position.Y <= 1.201f);
            REQUIRE(std::abs(v.normal.LengthSquared()-1) < 0.001f);
            if (v.position.Y > 0.17f) REQUIRE(std::abs(v.position.Z) > 10);
        }
    }
    MeshBuilder invalid;
    TrainSim::Render::addTerminalFrontage(invalid,{},0,30,0,1);
    REQUIRE(invalid.data().empty());
}

TEST_CASE("terminal streets retain bounded geometry and follow their ground callback")
{
    MeshBuilder mesh;
    TrainSim::Render::addTerminalStreet(mesh,[](float along,float across) {
        return Vector3{across,0.01f*along+0.02f*across,along};
    });
    REQUIRE(mesh.data().indices.size() == 2340);
    for (const auto& vertex : mesh.data().vertices)
    {
        const auto& p=vertex.position;
        REQUIRE(std::abs(p.X)<=6.101f);
        REQUIRE(std::abs(p.Z)<=325.001f);
        const float lift=p.Y-0.01f*p.Z-0.02f*p.X;
        REQUIRE(lift>=0.0499f && lift<=0.1601f);
        REQUIRE(std::abs(vertex.normal.LengthSquared()-1)<0.001f);
    }
}

TEST_CASE("terminal verge trees stay between terrain band and street with separated roots")
{
    for (unsigned seed : {7U,19U,348U,360U})
    {
        float previous=0;
        for (int index=0;index<10;++index)
        {
            const auto p=TrainSim::Render::terminalVergeTree(index,seed);
            const auto repeat=TrainSim::Render::terminalVergeTree(index,seed);
            REQUIRE(p.outward == repeat.outward);
            REQUIRE(p.lateral == repeat.lateral);
            REQUIRE(p.scale == repeat.scale);
            REQUIRE(p.outward >= 21 && p.outward <= 255);
            REQUIRE(p.outward-previous >= 2);
            REQUIRE(p.scale >= 0.899f && p.scale <= 1.201f);
            REQUIRE(p.lateral-2.4f*p.scale > 40);
            REQUIRE(p.lateral+2.4f*p.scale < 51.9f);
            previous=p.outward;
        }
    }
}

TEST_CASE("terminal terrain extends beyond service endpoints without extending every chunk")
{
    using TrainSim::Render::terminalTerrainRange;
    REQUIRE(terminalTerrainRange(0,250,286400).first == -1200);
    REQUIRE(terminalTerrainRange(0,250,286400).second == 250);
    REQUIRE(terminalTerrainRange(250,500,286400).first == 250);
    REQUIRE(terminalTerrainRange(250,500,286400).second == 500);
    REQUIRE(terminalTerrainRange(286250,286400,286400).first == 286250);
    REQUIRE(terminalTerrainRange(286250,286400,286400).second == 287600);
}

TEST_CASE("passenger head proportions preserve the animated joint and other bones")
{
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::GamerServices::AvatarBone;
    std::array<Matrix,71> bones;
    bones.fill(Matrix::getIdentityProperty());
    const auto head=static_cast<std::size_t>(AvatarBone::Head);
    const Matrix original=Matrix::CreateRotationY(0.4f)*Matrix::CreateTranslation(1,2,3);
    bones[head]=original;
    TrainSim::Render::applyPassengerProportions(bones);
    REQUIRE(bones[head].getTranslationProperty() == original.getTranslationProperty());
    const auto origin=Vector3::Transform(Vector3::Zero,bones[head]);
    for (const auto axis : {Vector3{1,0,0},Vector3{0,1,0},Vector3{0,0,1}})
        REQUIRE(std::abs((Vector3::Transform(axis,bones[head])-origin).Length()-0.74f) < 1e-5f);
    for (std::size_t i=0;i<bones.size();++i)
        if (i != head) REQUIRE(bones[i] == Matrix::getIdentityProperty());
    TrainSim::Render::applyPassengerProportions({});
}

TEST_CASE("blended avatar bones stay decomposable through a sit/stand transition")
{
    using namespace Microsoft::Xna::Framework;
    using TrainSim::Render::blendBone;
    // The stock pose's shoulder clasps the hands behind the back; the lap rest turns it far round.
    const Matrix native=Matrix::CreateRotationX(-1.9f)*Matrix::CreateRotationY(0.7f)
        *Matrix::CreateTranslation(0.18f,1.42f,0.02f);
    const Matrix rest=Matrix::CreateRotationZ(0.22f)*Matrix::CreateRotationX(0.15f)
        *Matrix::CreateTranslation(native.getTranslationProperty());
    Vector3 scale, translation;
    Quaternion rotation;
    REQUIRE(!Matrix::Lerp(native,rest,0.5f).Decompose(scale,rotation,translation));
    REQUIRE(blendBone(native,rest,0.0f) == native);
    REQUIRE(blendBone(native,rest,1.0f) == rest);
    for (const float amount : {0.1f,0.35f,0.5f,0.65f,0.9f})
    {
        const Matrix blended=blendBone(native,rest,amount);
        REQUIRE(blended.Decompose(scale,rotation,translation));
        REQUIRE(std::abs(scale.X-1.0f) < 1e-4f);
        REQUIRE(std::abs(scale.Y-1.0f) < 1e-4f);
        REQUIRE(std::abs(scale.Z-1.0f) < 1e-4f);
        REQUIRE((translation-native.getTranslationProperty()).Length() < 1e-5f);
    }
}

TEST_CASE("daylight sky closes both poles with finite nondegenerate triangles")
{
    MeshBuilder builder;
    TrainSim::Render::addDaylightSky(builder,1070);
    const auto mesh=builder.take();
    REQUIRE(mesh.indices.size() == 960*3);
    bool north=false,south=false;
    for (const auto& v : mesh.vertices)
    {
        REQUIRE(std::abs(v.position.Length()-1070) < 0.001f);
        north |= v.position.Y == 1070;
        south |= v.position.Y == -1070;
    }
    REQUIRE(north && south);
    for (std::size_t i=0;i<mesh.indices.size();i+=3)
    {
        const auto a=mesh.vertices[mesh.indices[i]].position;
        const auto b=mesh.vertices[mesh.indices[i+1]].position;
        const auto c=mesh.vertices[mesh.indices[i+2]].position;
        REQUIRE(Vector3::Cross(b-a,c-a).LengthSquared() > 1);
    }
    const auto horizon=TrainSim::Render::daylightSkyColor(0);
    const auto zenith=TrainSim::Render::daylightSkyColor(1);
    REQUIRE(horizon.getRProperty() > zenith.getRProperty());
    REQUIRE(horizon.getGProperty() > zenith.getGProperty());
    REQUIRE(TrainSim::Render::daylightSkyColor(-1) == horizon);
}

TEST_CASE("terrain shadow placement follows both triangles rather than a bilinear surface")
{
    const std::array<Vector3,4> corners{{{0,0,0},{0,0,10},{10,4,10},{10,0,0}}};
    const auto a=TrainSim::Render::terrainQuadPoint(corners,0.75f,0.25f);
    const auto b=TrainSim::Render::terrainQuadPoint(corners,0.25f,0.75f);
    const auto diagonal=TrainSim::Render::terrainQuadPoint(corners,0.5f,0.5f);
    REQUIRE((a-Vector3{2.5f,1,7.5f}).LengthSquared() < 1e-8f);
    REQUIRE((b-Vector3{7.5f,1,2.5f}).LengthSquared() < 1e-8f);
    REQUIRE((diagonal-Vector3{5,2,5}).LengthSquared() < 1e-8f);
}

TEST_CASE("vegetation shadows feather outward and face upward")
{
    MeshBuilder builder;
    TrainSim::Render::addSoftVegetationShadow(builder,
        [](float x,float z) { return Vector3{x,0,z}; },2,3,Color(10,20,15,40),7);
    const auto mesh=builder.take();
    REQUIRE(mesh.indices.size() == 108);
    int transparent=0;
    for (const auto& v : mesh.vertices)
    {
        REQUIRE(std::abs(v.position.X) <= 2 && std::abs(v.position.Z) <= 3);
        REQUIRE(v.normal.Y > 0.99f);
        Color colour;
        colour.setPackedValueProperty(v.color);
        REQUIRE(colour.getAProperty() == 0 || colour.getAProperty() == 40);
        transparent += colour.getAProperty() == 0;
    }
    REQUIRE(transparent == 24);
}

TEST_CASE("leafy hedges taper their ends and retain bounded terrain-following cutouts")
{
    MeshBuilder core,leaves;
    const auto ground=[](float x,float z) { return Vector3{x,0.08f*x+0.12f*z,z}; };
    TrainSim::Render::addTerrainHedge(core,ground,12,1,1,Color::Green,42,&leaves);
    REQUIRE(core.data().indices.size()/3==88+8*42);
    REQUIRE(leaves.data().indices.size()/3==104);
    float endHeight=0,centreHeight=0;
    for (const auto& v : core.data().vertices)
    {
        const auto p=v.position;
        const float above=p.Y-0.08f*p.X-0.12f*p.Z;
        REQUIRE(above>=-0.06f && above<=1.02f);
        if (std::abs(p.X)>5.99f) endHeight=std::max(endHeight,above);
        else centreHeight=std::max(centreHeight,above);
        REQUIRE(std::isfinite(v.textureCoordinate.X));
    }
    REQUIRE(endHeight<0.62f && centreHeight>0.70f);
    for (const auto& v : leaves.data().vertices)
    {
        const auto p=v.position;
        const float above=p.Y-0.08f*p.X-0.12f*p.Z;
        REQUIRE(std::abs(p.X)<=6.001f && std::abs(p.Z)<0.56f);
        REQUIRE(above>=-0.1f && above<1.2f);
        REQUIRE(std::abs(v.normal.Length()-1)<0.001f);
    }
}

TEST_CASE("hedge profiles have bounded uneven tops and outward side normals")
{
    MeshBuilder builder;
    TrainSim::Render::addHedgeSegment(builder,Vector3::Zero,9,0.8f,1,0,Color::Green,42);
    const auto mesh=builder.take();
    REQUIRE(mesh.indices.size() == 204);
    for (const auto& v : mesh.vertices)
    {
        REQUIRE(std::abs(v.position.X) <= 4.5f);
        REQUIRE(std::abs(v.position.Z) <= 0.4f);
        REQUIRE(v.position.Y >= 0 && v.position.Y <= 1);
        REQUIRE(std::abs(v.normal.LengthSquared()-1) < 1e-5f);
    }
    REQUIRE(mesh.vertices[0].normal.Z < -0.99f);
    REQUIRE(mesh.vertices[8].normal.Y > 0.9f);
    TrainSim::Render::addHedgeSegment(builder,Vector3::Zero,0,1,1,0,Color::Green,0);
    REQUIRE(builder.take().empty());
}

TEST_CASE("continuous hedges follow sloping ground without gaps or repeated sampling")
{
    MeshBuilder builder;
    int samples=0;
    TrainSim::Render::addTerrainHedge(builder,[&](float x,float z) {
        ++samples;
        return Vector3{x,0.12f*x+0.2f*z,z};
    },96,1.1f,0.95f,Color::Green,52);
    const auto mesh=builder.take();
    REQUIRE(samples == 65*6);
    REQUIRE(mesh.indices.size() == (64*10+8)*3);
    for (const auto& v : mesh.vertices)
    {
        const float aboveGround=v.position.Y-(0.12f*v.position.X+0.2f*v.position.Z);
        REQUIRE(aboveGround >= -1e-5f && aboveGround <= 0.95f);
        REQUIRE(std::abs(v.position.X) <= 48 && std::abs(v.position.Z) <= 0.55f);
    }
    // Consecutive sections share the exact same edge, with no internal end caps.
    for (int span=0;span<63;++span)
        for (int edge=0;edge<5;++edge)
        {
            const auto index=static_cast<std::size_t>((span*5+edge)*4);
            REQUIRE(mesh.vertices[index+3].position == mesh.vertices[index+20].position);
            REQUIRE(mesh.vertices[index+2].position == mesh.vertices[index+21].position);
            REQUIRE(mesh.vertices[index+3].color == mesh.vertices[index+20].color);
            REQUIRE(mesh.vertices[index+2].color == mesh.vertices[index+21].color);
        }
}

TEST_CASE("crop rows follow sloping ground with upward shoulders and a bounded budget")
{
    MeshBuilder mesh;
    TrainSim::Render::addTerrainCropRow(mesh,[](float x,float z) {
        return Vector3{x,0.12f*x+0.2f*z,z};
    },60,0.14f,0.38f,Color::Green,33);
    REQUIRE(mesh.data().indices.size()==(30*4+2)*3);
    for (const auto& v : mesh.data().vertices)
    {
        const float above=v.position.Y-0.12f*v.position.X-0.2f*v.position.Z;
        REQUIRE(above>=-0.0301f && above<=0.3801f);
        REQUIRE(std::abs(v.position.X)<=30.001f && std::abs(v.position.Z)<=0.0701f);
    }
    for (std::size_t i=0;i<30*8;++i) REQUIRE(mesh.data().vertices[i].normal.Y>0);
    MeshBuilder invalid;
    TrainSim::Render::addTerrainCropRow(invalid,[](float x,float z) { return Vector3{x,0,z}; },
        0,1,1,Color::Green,0);
    REQUIRE(invalid.data().empty());
}

TEST_CASE("route-aligned field hedges retain ground contact and upward crown normals")
{
    for (bool longitudinal : {false,true})
    {
        MeshBuilder mesh;
        TrainSim::Render::addRouteAlignedHedge(mesh,[](double distance,double lateral) {
            return Vector3{static_cast<float>(lateral),static_cast<float>(0.12*distance+0.2*lateral),
                           static_cast<float>(distance)};
        },100,40,30,0.8f,0.9f,longitudinal,Color::Green,77);
        for (const auto& v : mesh.data().vertices)
        {
            const float delta=v.position.Y-(0.12f*v.position.Z+0.2f*v.position.X);
            REQUIRE(delta>=-0.0801f && delta<=0.8201f);
            REQUIRE(std::abs(v.position.X-40)<(longitudinal ? 0.41f : 15.01f));
            REQUIRE(std::abs(v.position.Z-100)<(longitudinal ? 15.01f : 0.41f));
        }
        REQUIRE(mesh.data().vertices[8].normal.Y>0.8f);
        REQUIRE(mesh.data().indices.size()==(20*10+8)*3);
    }
}

TEST_CASE("hedgerow colour and silhouette variation is deterministic and bounded")
{
    MeshBuilder a,b;
    const auto ground=[](float x,float z) { return Vector3{x,0,z}; };
    TrainSim::Render::addTerrainHedge(a,ground,12,1,1,Color(80,120,60,255),19);
    TrainSim::Render::addTerrainHedge(b,ground,12,1,1,Color(80,120,60,255),19);
    const auto first=a.take(),second=b.take();
    REQUIRE(first.indices == second.indices);
    bool colourVaries=false,sideVaries=false;
    for (std::size_t i=0;i<first.vertices.size();++i)
    {
        const auto& v=first.vertices[i];
        REQUIRE(v.position == second.vertices[i].position);
        REQUIRE(v.color == second.vertices[i].color);
        REQUIRE(std::abs(v.position.Z) <= 0.5f);
        Color c; c.setPackedValueProperty(v.color);
        REQUIRE(c.getAProperty() == 255);
        colourVaries |= v.color != first.vertices[0].color;
        if (std::abs(v.position.Y) < 1e-6f)
            sideVaries |= std::abs(std::abs(v.position.Z)-std::abs(first.vertices[0].position.Z)) > 0.02f;
    }
    REQUIRE(colourVaries && sideVaries);
}

TEST_CASE("cached ellipsoid angles preserve the original parametric geometry")
{
    for (int slices : {3,7,16,65})
    {
        constexpr int stacks = 4;
        constexpr float pi = 3.14159265358979323846f;
        MeshBuilder cached, reference;
        const Vector3 centre{1,2,3}, radii{2,3,4};
        cached.addEllipsoid(centre,radii,slices,stacks,Color::White);
        const auto point = [&](float a,float b) {
            return centre+Vector3{radii.X*std::cos(a)*std::cos(b),radii.Y*std::sin(a),
                                  radii.Z*std::cos(a)*std::sin(b)};
        };
        for (int row=0;row<stacks;++row)
            for (int column=0;column<slices;++column)
            {
                const float a=-pi*0.5f+pi*static_cast<float>(row)/stacks;
                const float b=-pi*0.5f+pi*static_cast<float>(row+1)/stacks;
                const float c=2*pi*static_cast<float>(column)/static_cast<float>(slices);
                const float d=2*pi*static_cast<float>(column+1)/static_cast<float>(slices);
                const int shade=static_cast<int>(255*(0.80f+0.20f*std::cos(c)));
                const Color color(shade,shade,shade,255);
                if (row==0) reference.addTriangle(centre-Vector3{0,3,0},point(b,c),point(b,d),color);
                else if (row+1==stacks) reference.addTriangle(point(a,d),point(a,c),centre+Vector3{0,3,0},color);
                else reference.addQuad(point(b,c),point(b,d),point(a,d),point(a,c),color);
            }
        const auto a=cached.take(), b=reference.take();
        REQUIRE(a.indices == b.indices);
        REQUIRE(a.vertices.size() == b.vertices.size());
        for (std::size_t i=0;i<a.vertices.size();++i)
        {
            REQUIRE((a.vertices[i].position-b.vertices[i].position).LengthSquared() < 1e-10f);
            REQUIRE((a.vertices[i].normal-b.vertices[i].normal).LengthSquared() < 1e-8f);
            REQUIRE(a.vertices[i].color == b.vertices[i].color);
        }
    }
}

TEST_CASE("mesh staging clear reuses capacity without retaining old geometry")
{
    MeshBuilder builder;
    builder.reserve(100,200);
    builder.addBox(Vector3::Zero,Vector3::One,Color::White);
    const auto vertexCapacity = builder.data().vertices.capacity();
    const auto indexCapacity = builder.data().indices.capacity();
    builder.clear();
    REQUIRE(builder.data().empty());
    REQUIRE(builder.data().vertices.empty());
    REQUIRE(builder.data().vertices.capacity() == vertexCapacity);
    REQUIRE(builder.data().indices.capacity() == indexCapacity);
    builder.addTriangle({0,0,0},{1,0,0},{0,1,0},Color::White);
    REQUIRE(builder.data().vertices.size() == 3);
    REQUIRE(builder.data().indices.size() == 3);
    REQUIRE(builder.data().indices[0] == 0);
}

TEST_CASE("clustered crowns are deterministic and bounded by the maintained corridor envelope")
{
    for (std::uint32_t seed : {0U,31U,96U,238000U,0xffffffffU,0x9e3779b9U})
    for (bool reduced : {false,true})
    {
        MeshBuilder first, second;
        TrainSim::Render::addClusteredBroadleafCrown(first,Vector3::Zero,1,seed,Color::Green,Color::White,reduced);
        TrainSim::Render::addClusteredBroadleafCrown(second,Vector3::Zero,1,seed,Color::Green,Color::White,reduced);
        const auto a = first.take(), b = second.take();
        REQUIRE(a.indices.size() == (reduced ? 972 : 1890)); // Nine 36- or fifteen 42-triangle lobes.
        REQUIRE(a.indices == b.indices);
        REQUIRE(a.vertices.size() == b.vertices.size());
        for (std::size_t i = 0; i < a.vertices.size(); ++i)
        {
            const auto& v = a.vertices[i];
            REQUIRE(std::abs(v.position.X) <= 2.22f);
            REQUIRE(std::abs(v.position.Z) <= 1.94f);
            REQUIRE(v.position.Y >= 2.66f && v.position.Y <= 6.32f);
            REQUIRE((v.position-b.vertices[i].position).LengthSquared() == 0);
            REQUIRE(v.color == b.vertices[i].color);
        }
    }
}

TEST_CASE("field colour subdivision preserves both planes of non-planar terrain cells")
{
    MeshBuilder cell;
    const std::array<Vector3,4> corners={Vector3{0,0,0},Vector3{0,2,50},
                                        Vector3{40,7,50},Vector3{40,-1,0}};
    TrainSim::Render::addSubdividedTerrainCell(cell,corners,
        [](float,float) { return Color::Green; },[](float u,float v) { return Vector2{v*5,u*6.25f}; });
    REQUIRE(cell.data().indices.size()/3==18);
    for (const auto& v : cell.data().vertices)
    {
        const float u=v.position.Z/50,w=v.position.X/40;
        REQUIRE((v.position-TrainSim::Render::terrainQuadPoint(corners,u,w)).Length()<0.00001f);
        REQUIRE(std::abs(v.textureCoordinate.X-v.position.X/8)<0.00001f);
        REQUIRE(std::abs(v.textureCoordinate.Y-v.position.Z/8)<0.00001f);
        REQUIRE(v.normal.Y>0.9f);
    }
    for (std::size_t i=0;i<cell.data().indices.size();i+=3)
    {
        const auto& m=cell.data();
        const auto centre=(m.vertices[m.indices[i]].position+m.vertices[m.indices[i+1]].position
                         +m.vertices[m.indices[i+2]].position)/3;
        REQUIRE((centre-TrainSim::Render::terrainQuadPoint(corners,centre.Z/50,centre.X/40)).Length()<0.00001f);
    }
}

TEST_CASE("verge grass forms three bounded clusters with stable absolute-route placement")
{
    using namespace TrainSim::Render;
    for (int quality : {0,1,2})
        for (int side : {0,1})
            for (double distance : {0.0,52002.0,286398.0})
            {
                const int count=vergeBladeCount(quality);
                REQUIRE(count==(quality==0 ? 3 : quality==1 ? 9 : 15));
                const int perCluster=count/3;
                for (int blade=0;blade<count;++blade)
                {
                    const auto p=vergeBladePlacement(distance,side,quality,blade);
                    const auto repeat=vergeBladePlacement(distance,side,quality,blade);
                    REQUIRE(p.distance==repeat.distance && p.lateral==repeat.lateral);
                    const double centre=distance+(blade/perCluster-1)*1.05;
                    REQUIRE(std::abs(p.distance-centre)<=0.12001);
                    REQUIRE(std::abs(p.lateral)>10.69 && std::abs(p.lateral)<11.91);
                    REQUIRE((p.lateral<0)==(side==0));
                    REQUIRE(p.height>0.12f && p.height<=0.521f);
                    REQUIRE(p.width>=0.024f && p.width<=0.0651f);
                    REQUIRE(std::abs(p.yaw)<=3.142f);
                }
            }
    REQUIRE(vergeTuftStep(2)==3 && vergeTuftStep(1)==6 && vergeTuftStep(0)==50);
}

TEST_CASE("leafy shrubs remain grounded bounded and deterministic without per-shrub batches")
{
    for (unsigned seed : {0U,96U,12345U,0xffffffffU})
    {
        MeshBuilder core,leaves,repeatedCore,repeatedLeaves;
        const Vector3 ground{8,-2,13};
        TrainSim::Render::addLeafyShrub(core,leaves,ground,1,seed,Color::Green);
        TrainSim::Render::addLeafyShrub(repeatedCore,repeatedLeaves,ground,1,seed,Color::Green);
        REQUIRE(core.data().indices.size()/3==126);
        REQUIRE(leaves.data().indices.size()/3==36);
        for (std::size_t i=0;i<leaves.data().vertices.size();++i)
        {
            const auto& v=leaves.data().vertices[i];
            const auto p=v.position-ground;
            REQUIRE(std::abs(p.X)<1.05f && std::abs(p.Z)<0.78f);
            REQUIRE(p.Y>=0.0059f && p.Y<=1.151f);
            REQUIRE(std::abs(v.normal.Length()-1)<0.001f);
            REQUIRE(v.position==repeatedLeaves.data().vertices[i].position);
            REQUIRE(v.color==repeatedLeaves.data().vertices[i].color);
        }
        for (std::size_t i=0;i<core.data().vertices.size();++i)
            REQUIRE(core.data().vertices[i].position==repeatedCore.data().vertices[i].position);
    }
    MeshBuilder core,leaves;
    TrainSim::Render::addLeafyShrub(core,leaves,{},0,1,Color::Green);
    REQUIRE(core.data().vertices.empty() && leaves.data().vertices.empty());
}

TEST_CASE("broadleaf solid cores leave room for porous outer sprays")
{
    for (bool reduced : {false,true})
    {
        MeshBuilder crown;
        TrainSim::Render::addClusteredBroadleafCrown(crown,Vector3::Zero,1,12345,
                                                   Color::Green,Color::White,reduced);
        std::size_t cursor=0;
        TrainSim::Render::forEachBroadleafLobe(Vector3::Zero,1,12345,reduced,
            [&](const auto& centre,const auto& radii,float,bool) {
                MeshBuilder lobe;
                lobe.addFoliageLobe(centre,radii,reduced ? 6 : 7,4,Color::Green,true);
                for (std::size_t i=0;i<lobe.data().vertices.size();++i)
                {
                    const auto p=crown.data().vertices[cursor++].position-centre;
                    const Vector3 unit{p.X/radii.X,p.Y/radii.Y,p.Z/radii.Z};
                    REQUIRE(unit.Length()<=0.6001f);
                }
            });
        REQUIRE(cursor==crown.data().vertices.size());
    }
}

TEST_CASE("leaf edge sprays keep a bounded double-sided geometry budget")
{
    for (bool reduced:{false,true})
    {
        MeshBuilder mesh;
        TrainSim::Render::addBroadleafEdgeSprays(mesh,Vector3::Zero,1,12345,Color::Green,reduced);
        REQUIRE(mesh.data().indices.size()==(reduced ? 108 : 180)*3);
        for (const auto& v:mesh.data().vertices)
        {
            REQUIRE(std::abs(v.position.X)<2.22f && std::abs(v.position.Z)<2.22f);
            REQUIRE(v.position.Y>2.66f && v.position.Y<6.32f);
            REQUIRE(v.textureCoordinate.X>=0 && v.textureCoordinate.X<=1);
            REQUIRE(v.textureCoordinate.Y>=0 && v.textureCoordinate.Y<=1);
        }
        std::size_t first=0;
        TrainSim::Render::forEachBroadleafLobe(Vector3::Zero,1,12345,reduced,
            [&](const auto& lobe,const auto&,float,bool) {
                for (int plane=0;plane<3;++plane)
                {
                    Vector3 centre=Vector3::Zero;
                    for (std::size_t i=0;i<4;++i) centre+=mesh.data().vertices[first+i].position*0.25f;
                    REQUIRE((centre-lobe).Length()<0.00001f);
                    first+=8; // Three crossed, double-sided planes per lobe.
                }
            });
        REQUIRE(first==mesh.data().vertices.size());
    }
}

TEST_CASE("broadleaf shape identity survives a floating-origin translation")
{
    MeshBuilder first, shifted, different;
    const auto seed=TrainSim::Render::treeShapeSeed(52000,35,1);
    const Vector3 offset{123,2,-87};
    TrainSim::Render::addClusteredBroadleafCrown(first,Vector3::Zero,1,seed,Color::Green,Color::White);
    TrainSim::Render::addClusteredBroadleafCrown(shifted,offset,1,seed,Color::Green,Color::White);
    TrainSim::Render::addClusteredBroadleafCrown(different,Vector3::Zero,1,
        TrainSim::Render::treeShapeSeed(52000,-35,1),Color::Green,Color::White);
    REQUIRE(first.data().indices == shifted.data().indices);
    REQUIRE(first.data().indices == different.data().indices);
    std::size_t changed=0;
    for (std::size_t i=0;i<first.data().vertices.size();++i)
    {
        const auto& original=first.data().vertices[i];
        REQUIRE((shifted.data().vertices[i].position-offset-original.position).Length()<0.00003f);
        REQUIRE(original.color == shifted.data().vertices[i].color);
        if ((different.data().vertices[i].position-original.position).Length()>0.01f) ++changed;
    }
    REQUIRE(changed>100);
}

TEST_CASE("lake far bank relief varies smoothly without changing the near shore")
{
    using TrainSim::Render::lakeFarBankRelief;
    double minimum=20,maximum=0;
    for (double distance=80500;distance<=87200;distance+=25)
    {
        REQUIRE(lakeFarBankRelief(distance,90)==0);
        REQUIRE(lakeFarBankRelief(distance,-600)==0);
        REQUIRE(lakeFarBankRelief(distance,455)==0);
        const double height=lakeFarBankRelief(distance,650);
        REQUIRE(height>=0 && height<=38);
        REQUIRE(std::abs(height-lakeFarBankRelief(distance+1,650))<0.30);
        minimum=std::min(minimum,height);
        maximum=std::max(maximum,height);
    }
    REQUIRE(maximum-minimum>8);
    REQUIRE(lakeFarBankRelief(80500,650)==0);
    REQUIRE(lakeFarBankRelief(87200,650)==0);
}

TEST_CASE("far lake trees form stable bounded groves instead of a regular line")
{
    using TrainSim::Render::lakeBankTreePlacement;
    double minimumLateral=1000, maximumLateral=0;
    for (std::int64_t stripe=1600;stripe<1640;++stripe)
        for (int slot=0;slot<4;++slot)
        {
            const auto p=lakeBankTreePlacement(stripe,slot);
            const auto repeat=lakeBankTreePlacement(stripe,slot);
            REQUIRE(p.distance==repeat.distance);
            REQUIRE(p.lateral==repeat.lateral);
            REQUIRE(p.scale==repeat.scale);
            const double groupBegin=static_cast<double>(stripe/2)*100;
            REQUIRE(p.distance>=groupBegin+11 && p.distance<=groupBegin+89);
            REQUIRE(p.lateral>=545 && p.lateral<=656);
            REQUIRE(p.scale>=1.45f && p.scale<=2.4f);
            REQUIRE(p.variant<3);
            minimumLateral=std::min(minimumLateral,p.lateral);
            maximumLateral=std::max(maximumLateral,p.lateral);
            if (slot<3) REQUIRE(lakeBankTreePlacement(stripe,slot+1).distance-p.distance>=2);
        }
    REQUIRE(maximumLateral-minimumLateral>60);
}

TEST_CASE("lake approach stays below the passenger sightline and preserves the far bank")
{
    using TrainSim::Render::lakeShoreTerrainOffset;
    for (double natural : {-0.34,3.0,12.0})
    {
        REQUIRE(lakeShoreTerrainOffset(natural,-100,-9)==natural);
        REQUIRE(lakeShoreTerrainOffset(natural,12,-9)==natural);
        for (double lateral : {26.0,40.0,60.0,90.0,100.0})
            REQUIRE(lakeShoreTerrainOffset(natural,lateral,-9)<=-0.34);
        REQUIRE(std::abs(lakeShoreTerrainOffset(natural,150,-9)+11.4)<1e-8);
        REQUIRE(std::abs(lakeShoreTerrainOffset(natural,600,-9)-natural)<1e-8);
    }
}

TEST_CASE("pitched roof material follows slope length and leaves nondegenerate gables")
{
    MeshBuilder walls, roof;
    TrainSim::Render::addPitchedRoof(walls,roof,0,6,0,8,12,3,0,Color::White);
    REQUIRE(roof.data().indices.size()==48); // Slopes plus six ridge strips.
    REQUIRE(walls.data().indices.size()==114); // Gables, closed soffit and both eave gutters.
    float maxU=0,maxV=0;
    for (const auto& v : roof.data().vertices)
    {
        REQUIRE(v.normal.Y>0);
        maxU=std::max(maxU,v.textureCoordinate.X);
        maxV=std::max(maxV,v.textureCoordinate.Y);
    }
    REQUIRE(std::abs(maxU-10)<1e-5f);
    REQUIRE(std::abs(maxV-3.125f)<1e-5f);
    float ridgeTop=0;
    for (const auto& vertex : roof.data().vertices) ridgeTop=std::max(ridgeTop,vertex.position.Y);
    REQUIRE(std::abs(ridgeTop-9.08f)<1e-5f);
    REQUIRE(roof.data().vertices[8].position.Y<9.0f-0.18f*3.0f/4.0f);
    for (std::size_t i=0;i<6;i+=3) // The first two triangles are the gables.
    {
        const auto& m=walls.data();
        const auto a=m.vertices[m.indices[i]].position;
        const auto b=m.vertices[m.indices[i+1]].position;
        const auto c=m.vertices[m.indices[i+2]].position;
        REQUIRE(Vector3::Cross(b-a,c-a).LengthSquared()>1);
    }
    MeshBuilder invalid;
    TrainSim::Render::addPitchedRoof(invalid,invalid,0,0,0,0,12,3,0,Color::White);
    REQUIRE(invalid.data().empty());
}

TEST_CASE("flat roof finish stays inside a bounded coping and above the existing slab")
{
    MeshBuilder structure,surface;
    TrainSim::Render::addFlatRoofFinish(structure,surface,0,10,0,8,12,0,2);
    REQUIRE(surface.data().indices.size()==6);
    REQUIRE(structure.data().indices.size()<650);
    for (const auto& v : surface.data().vertices)
    {
        REQUIRE(v.normal.Y>0.999f);
        REQUIRE(std::abs(v.position.Y-10.012f)<1e-4f);
        REQUIRE(std::abs(v.position.X)<=3.831f);
        REQUIRE(std::abs(v.position.Z)<=5.831f);
        REQUIRE(v.textureCoordinate.X>=0 && v.textureCoordinate.Y>=0);
    }
    for (const auto& v : structure.data().vertices)
    {
        REQUIRE(std::isfinite(v.normal.X));
        REQUIRE(std::abs(v.normal.LengthSquared()-1)<1e-5f);
        REQUIRE(v.position.Y>=9.959f && v.position.Y<=10.791f);
        REQUIRE(std::abs(v.position.X)<=4.091f);
        REQUIRE(std::abs(v.position.Z)<=6.091f);
    }
    MeshBuilder invalid;
    TrainSim::Render::addFlatRoofFinish(invalid,invalid,0,0,0,1,1,0,0);
    REQUIRE(invalid.data().empty());
}

TEST_CASE("domestic gutters and connected downpipes fit the eaves and terminate above ground")
{
    using Microsoft::Xna::Framework::Matrix;
    MeshBuilder mesh;
    TrainSim::Render::addDomesticRainwaterGoods(mesh,Matrix::getIdentityProperty(),7.7f,10.8f,7,6);
    REQUIRE(mesh.data().indices.size()>900);
    REQUIRE(mesh.data().indices.size()<2000);
    bool leftOutlet=false,rightOutlet=false,openTrough=false;
    for (const auto& v : mesh.data().vertices)
    {
        REQUIRE(std::isfinite(v.normal.X));
        REQUIRE(std::abs(v.normal.LengthSquared()-1)<1e-4f);
        REQUIRE(std::abs(v.position.X)<4.0f);
        REQUIRE(std::abs(v.position.Z)<=5.401f);
        REQUIRE(v.position.Y>=-5.95f && v.position.Y<=0.001f);
        leftOutlet=leftOutlet || (v.position.X<0 && v.position.Y<-5.8f);
        rightOutlet=rightOutlet || (v.position.X>0 && v.position.Y<-5.8f);
        openTrough=openTrough || (v.position.Y>-0.14f && v.normal.Y>0.5f);
    }
    REQUIRE(leftOutlet && rightOutlet && openTrough);
    int pipeFaces=0;
    const auto& data=mesh.data();
    for (std::size_t i=0;i<data.indices.size();i+=3)
    {
        const auto& a=data.vertices[data.indices[i]];
        const auto& b=data.vertices[data.indices[i+1]];
        const auto& c=data.vertices[data.indices[i+2]];
        const auto centre=(a.position+b.position+c.position)/3.0f;
        if (a.color!=Color(89,96,94,255).getPackedValueProperty()
            || centre.Y>=-1 || centre.Y<=-5) continue;
        const float pipeX=centre.X<0 ? -3.58f : 3.58f;
        REQUIRE(Vector3::Dot(a.normal,{centre.X-pipeX,0,centre.Z-4.95f})>0.01f);
        ++pipeFaces;
    }
    REQUIRE(pipeFaces>=16);
    MeshBuilder invalid;
    TrainSim::Render::addDomesticRainwaterGoods(invalid,Matrix::getIdentityProperty(),7,10,8,6);
    REQUIRE(invalid.data().empty());
}

TEST_CASE("detailed pitched roofs close the full overhang underside")
{
    MeshBuilder walls,roof;
    TrainSim::Render::addPitchedRoof(walls,roof,0,6,0,8,12,3,0,Color::White,7,6);
    int undersideCorners=0;
    for (const auto& v : walls.data().vertices)
        if (v.normal.Y<-0.999f && std::abs(v.position.Y-5.945f)<1e-5f)
        {
            REQUIRE(std::abs(std::abs(v.position.X)-4.0f)<1e-5f);
            REQUIRE(std::abs(std::abs(v.position.Z)-6.0f)<1e-5f);
            ++undersideCorners;
        }
    REQUIRE(undersideCorners==4);
}

TEST_CASE("residential roof envelopes track scale and every building height variant")
{
    for (float scale : {0.7f,1.0f,1.4f})
    {
        const auto house=TrainSim::Render::houseEnvelope(scale);
        REQUIRE(std::abs(house.roofHeight-8.15f*scale)<1e-5f);
        REQUIRE(std::abs(house.roofHeight-house.wallHeight-2.15f*scale)<1e-5f);
        REQUIRE(house.roofWidth>house.width);
        REQUIRE(house.roofLength>house.length);
        for (unsigned variant=0;variant<12;++variant)
        {
            const auto town=TrainSim::Render::townhouseEnvelope(scale,variant);
            REQUIRE(std::abs(town.wallHeight-(9.2f+1.35f*static_cast<float>(variant%3U))*scale)<1e-5f);
            REQUIRE(std::abs(town.roofHeight-town.wallHeight-0.44f*scale)<1e-5f);
            REQUIRE(town.roofWidth>town.width);
            REQUIRE(town.roofLength>town.length);
            const auto apartment=TrainSim::Render::apartmentEnvelope(scale,variant);
            // Plan bits 3-4 == 1 select the long four-storey terrace (ENV-134).
            const bool terrace=((variant>>3U)&3U)==1U;
            REQUIRE(std::abs(apartment.wallHeight-(terrace ? 10.5f
                : 13.0f+2.2f*static_cast<float>(variant%4U))*scale)<1e-5f);
            REQUIRE(std::abs(apartment.roofHeight-apartment.wallHeight-0.5f)<1e-5f);
            REQUIRE(std::abs(apartment.width-10.0f*scale)<1e-5f);
            REQUIRE(std::abs(apartment.length-(terrace ? 19.0f : 13.0f)*scale)<1e-5f);
            REQUIRE(apartment.roofWidth>apartment.width);
            REQUIRE(apartment.roofLength>apartment.length);
            const int floors=TrainSim::Render::apartmentFloorCount(variant);
            REQUIRE(floors==(terrace ? 4 : 5+static_cast<int>(variant%4U)));
            // The top window row stays below the eaves for every plan.
            REQUIRE((2.0f+static_cast<float>(floors-1)*2.35f+0.45f)*scale<apartment.wallHeight);
            const float topWindowEdge=(2.0f+static_cast<float>(floors-1)*2.35f+0.54f)*scale;
            const float roofClearance=(apartment.wallHeight-topWindowEdge)/scale;
            REQUIRE(roofClearance>0.5f);
            REQUIRE(roofClearance<1.2f);
        }
    }
}

TEST_CASE("townhouse storeys fill the facade while keeping realistic unscaled floor spacing")
{
    for (unsigned variant=0;variant<12;++variant)
        for (float scale : {0.7f,1.0f,1.4f})
        {
            const auto envelope=TrainSim::Render::townhouseEnvelope(scale,variant);
            const auto storeys=TrainSim::Render::townhouseStoreys(scale,variant);
            REQUIRE(storeys.count==(variant%3U==0U ? 3 : 4));
            REQUIRE(storeys.pitch/scale>2.6f);
            REQUIRE(storeys.pitch/scale<3.1f);
            REQUIRE(std::abs(storeys.windowHeight-1.35f*scale)<1e-5f);
            // Frame rails extend 90 mm beyond the pane, independent of building scale.
            const float halfFrame=storeys.windowHeight*0.5f+0.09f;
            const float top=storeys.windowCentre(storeys.count-1)+halfFrame;
            REQUIRE((envelope.wallHeight-top)/scale>0.25f);
            REQUIRE((envelope.wallHeight-top)/scale<0.50f);
            REQUIRE(storeys.windowCentre(0)-halfFrame>0.75f*scale);
            for (int floor=1;floor<storeys.count;++floor)
                REQUIRE(std::abs(storeys.windowCentre(floor)-storeys.windowCentre(floor-1)
                                 -storeys.pitch)<1e-5f);
            const auto unit=TrainSim::Render::townhouseStoreys(1.0f,variant);
            REQUIRE(std::abs(storeys.windowCentre(1)-unit.windowCentre(1)*scale)<1e-5f);
        }
}

TEST_CASE("projected building shadows preserve area in every sunlight quadrant")
{
    for (float dx : {-4.0f,0.0f,4.0f})
        for (float dz : {-3.0f,0.0f,3.0f})
        {
            MeshBuilder builder;
            int samples=0;
            TrainSim::Render::addProjectedBoxShadow(builder,6,8,dx,dz,
                [&](float x,float z) { ++samples; return Vector3{x,0.1f*x+0.03f*z,z}; },Color::Black);
            const auto mesh=builder.take();
            REQUIRE(mesh.indices.size()<=288);
            const auto sectors=mesh.indices.size()/48;
            REQUIRE(samples==1+static_cast<int>(sectors)*10);
            double area=0;
            for (std::size_t i=0;i<mesh.indices.size();i+=3)
            {
                const auto a=mesh.vertices[mesh.indices[i]].position;
                const auto b=mesh.vertices[mesh.indices[i+1]].position;
                const auto c=mesh.vertices[mesh.indices[i+2]].position;
                area+=std::abs((b.X-a.X)*(c.Z-a.Z)-(b.Z-a.Z)*(c.X-a.X))*0.5;
            }
            REQUIRE(std::abs(area-(48+8*std::abs(dx)+6*std::abs(dz)))<0.001);
            for (const auto& v : mesh.vertices)
            {
                REQUIRE(v.normal.Y>0.99f);
                REQUIRE(std::abs(v.position.Y-(0.1f*v.position.X+0.03f*v.position.Z))<1e-5f);
            }
        }
}

TEST_CASE("street pocket paving mirrors without downward faces and stays within its reservation")
{
    for (int side : {-1,1})
    {
        MeshBuilder builder;
        TrainSim::Render::addStreetPocket(builder,[&](float x,float z) {
            return Vector3{static_cast<float>(side)*x,0,z};
        },0,side);
        const auto& mesh=builder.data();
        REQUIRE(mesh.indices.size()<2200);
        for (const auto& v : mesh.vertices)
        {
            REQUIRE(std::abs(v.position.X)<=5.401f);
            REQUIRE(std::abs(v.position.Z)<=6.001f);
            REQUIRE(v.position.Y>=-0.101f);
            REQUIRE(v.position.Y<1.13f);
            if (v.position.Y>0.046f)
                REQUIRE(std::abs(v.position.X)>2.3f || std::abs(v.position.Z)>3.9f);
        }
        for (std::size_t i=0;i<100;++i) REQUIRE(mesh.vertices[i].normal.Y>0.99f);
    }
}

TEST_CASE("station entrance stays within its doorway and keeps hardware platform-facing")
{
    using Microsoft::Xna::Framework::Matrix;
    for (float yaw : {-1.570796327f,0.0f,1.570796327f})
    {
        MeshBuilder builder;
        const auto transform=Matrix::CreateRotationY(yaw)*Matrix::CreateTranslation(10,0.784f,20);
        TrainSim::Render::addStationEntrance(builder,transform);
        const auto& mesh=builder.data();
        REQUIRE(!mesh.empty());
        REQUIRE(mesh.indices.size()<1000);
        int handleVertices=0;
        const auto inverse=Matrix::Invert(transform);
        for (const auto& vertex : mesh.vertices)
        {
            const auto p=Vector3::Transform(vertex.position,inverse);
            REQUIRE(std::abs(p.X)<=0.811f);
            REQUIRE(p.Y>=-0.00001f);
            REQUIRE(p.Y<=2.481f);
            REQUIRE(p.Z>=-0.036f);
            REQUIRE(p.Z<=0.151f);
            if (p.Z>0.11f && p.Y>0.9f && p.Y<1.5f) ++handleVertices;
        }
        REQUIRE(handleVertices>=8);
    }
}

TEST_CASE("station storeys start at platform level and reserve the actual entrance bay")
{
    for (float height : {4.8f,9.5f,10.5f})
    {
        const auto floors=TrainSim::Render::stationStoreys(height,0.78f);
        REQUIRE(floors.count==(height<8 ? 1 : 3));
        REQUIRE(floors.pitch>2.8f);
        for (int floor=0;floor<floors.count;++floor)
        {
            const float centre=floors.windowCentre(floor);
            REQUIRE(centre-floors.WindowHeight*0.5f>0.78f+0.8f);
            REQUIRE(centre+floors.WindowHeight*0.5f+0.09f<height-0.25f);
        }
    }
    for (double length : {16.0,42.0,58.0})
        for (double side : {-1.0,1.0})
        {
            const double door=length*0.07*side;
            REQUIRE(!TrainSim::Render::stationWindowClearsEntrance(door,door));
            REQUIRE(!TrainSim::Render::stationWindowClearsEntrance(door+1.8,door));
            REQUIRE(TrainSim::Render::stationWindowClearsEntrance(door+1.9,door));
            REQUIRE(TrainSim::Render::stationWindowClearsEntrance(door-1.9,door));
        }
}

TEST_CASE("facade end finishes preserve outward normals and physical texture scale")
{
    using Microsoft::Xna::Framework::Matrix;
    for (float yaw : {0.0f,0.7f,-1.2f})
    {
        MeshBuilder builder;
        const auto transform=Matrix::CreateRotationY(yaw)*Matrix::CreateTranslation(12,3,-7);
        TrainSim::Render::addFacadeEndMaterials(builder,transform,6,9.2f,0.04f,10,Color::White);
        const auto& mesh=builder.data();
        REQUIRE(mesh.vertices.size()==8);
        REQUIRE(mesh.indices.size()==12);
        const auto inverse=Matrix::Invert(transform);
        for (const auto& vertex : mesh.vertices)
        {
            const auto local=Vector3::Transform(vertex.position,inverse);
            const auto normal=Vector3::TransformNormal(vertex.normal,inverse);
            REQUIRE(std::abs(std::abs(local.Z)-4.6f)<0.00001f);
            REQUIRE(normal.Z*local.Z>4.59f);
            REQUIRE(std::abs(vertex.textureCoordinate.X-local.X/2)<0.00001f);
            REQUIRE(std::abs(vertex.textureCoordinate.Y-local.Y/2)<0.00001f);
        }
    }
    MeshBuilder invalid;
    TrainSim::Render::addFacadeEndMaterials(invalid,Matrix::CreateRotationY(0),0,9,0,10,Color::White);
    REQUIRE(invalid.data().empty());
}

TEST_CASE("townhouse front gardens respect parcels and keep the entrance opening clear")
{
    for (float side : {-1.0f,1.0f})
        for (unsigned variant : {0U,1U,6U,7U})
        {
            MeshBuilder builder;
            TrainSim::Render::addTownhouseFrontGarden(builder,Vector3::Zero,1,0,side,variant);
            const auto& mesh=builder.data();
            REQUIRE(!mesh.empty());
            REQUIRE(mesh.indices.size()<1200);
            const float door=variant%2U==0U ? -1.55f : 1.55f;
            for (const auto& vertex : mesh.vertices)
            {
                const auto& p=vertex.position;
                REQUIRE(-p.X*side>=3.39f);
                REQUIRE(-p.X*side<=4.30f);
                REQUIRE(std::abs(p.Z)<=5.23f);
                REQUIRE(std::abs(p.Z-door)>=0.919f);
                REQUIRE(p.Y>=0.063f);
                REQUIRE(p.Y<=1.001f);
                REQUIRE(std::abs(vertex.normal.LengthSquared()-1.0f)<1e-5f);
            }
        }
    MeshBuilder invalid;
    TrainSim::Render::addTownhouseFrontGarden(invalid,Vector3::Zero,0,0,1,0);
    REQUIRE(invalid.data().empty());
    for (float scale : {0.76f,1.18f})
    {
        MeshBuilder scaled;
        TrainSim::Render::addTownhouseFrontGarden(scaled,{0,3,0},scale,0,1,0);
        float bottom=100;
        for (const auto& vertex : scaled.data().vertices)
            bottom=std::min(bottom,vertex.position.Y);
        REQUIRE(std::abs(bottom-3.064f)<1e-5f);
    }
}

TEST_CASE("window planters meet the sill and overlap either facade side")
{
    for (float side : {-1.0f,1.0f})
    {
        MeshBuilder builder;
        TrainSim::Render::addWindowPlanter(builder,Vector3::Zero,4,1,0,side);
        const auto mesh=builder.take();
        REQUIRE(mesh.indices.size() == 108);
        float minimum=1, maximum=-1;
        for (const auto& v : mesh.vertices)
        {
            const float outward=v.position.X*side;
            minimum=std::min(minimum,outward);
            maximum=std::max(maximum,outward);
            REQUIRE(outward>=-0.061f && outward<=0.301f);
            REQUIRE(v.position.Y>=3.699f && v.position.Y<=4.101f);
            REQUIRE(std::abs(v.position.Z)<=0.561f);
        }
        REQUIRE(minimum<0);
        REQUIRE(maximum>0.29f);
        // First box is the container, not its thin soil/foliage fill.
        float containerTop=0;
        for (std::size_t i=0;i<24;++i) containerTop=std::max(containerTop,mesh.vertices[i].position.Y);
        REQUIRE(containerTop==4);
    }
}

TEST_CASE("raised facade frames leave an open pane and stand proud of glazing")
{
    using Microsoft::Xna::Framework::Matrix;
    MeshBuilder builder;
    TrainSim::Render::addRaisedWindowFrame(builder,1.2f,1.4f,
        Matrix::getIdentityProperty(),Color::White);
    const auto mesh=builder.take();
    REQUIRE(mesh.indices.size() == 144);
    for (const auto& v : mesh.vertices)
    {
        REQUIRE(std::abs(v.position.X) <= 0.701f);
        REQUIRE(std::abs(v.position.Y) <= 0.791f);
        REQUIRE(std::abs(v.position.X) >= 0.599f || std::abs(v.position.Y) >= 0.689f);
        REQUIRE(std::abs(v.position.Z) > 0.0675f);
        REQUIRE(std::abs(v.position.Z) <= 0.111f);
        REQUIRE(std::abs(v.normal.LengthSquared()-1) < 1e-5f);
    }
    MeshBuilder empty;
    TrainSim::Render::addRaisedWindowFrame(empty,0,1,Matrix::getIdentityProperty(),Color::White);
    REQUIRE(empty.data().empty());
}

TEST_CASE("foliage deformation preserves topology and survives origin translation")
{
    for (int slices : {6,9})
    {
        MeshBuilder original, smooth, moved, low;
        const Vector3 radii{2,3,2};
        const Color green(60,100,50,255);
        original.addEllipsoid(Vector3::Zero,radii,slices,4,green);
        smooth.addFoliageLobe(Vector3::Zero,radii,slices,4,green,true);
        moved.addFoliageLobe({0,0,128},radii,slices,4,green,true);
        low.addFoliageLobe(Vector3::Zero,radii,slices,4,green,false);
        const auto a=original.take(), b=smooth.take(), c=moved.take(), d=low.take();
        REQUIRE(a.indices == b.indices);
        REQUIRE(a.vertices.size() == b.vertices.size());
        int changedColors = 0;
        int changedPositions = 0;
        for (std::size_t i=0;i<a.vertices.size();++i)
        {
            REQUIRE((a.vertices[i].position-d.vertices[i].position).LengthSquared() < 1e-8f);
            const float contraction=b.vertices[i].position.Length()/a.vertices[i].position.Length();
            REQUIRE(contraction>=0.83999f && contraction<=1.00001f);
            REQUIRE((c.vertices[i].position-Vector3{0,0,128}-b.vertices[i].position).Length()<0.00003f);
            REQUIRE(Vector3::Dot(b.vertices[i].normal,b.vertices[i].position)>0);
            changedPositions += (a.vertices[i].position-b.vertices[i].position).Length()>0.001f;
            REQUIRE(a.vertices[i].color == d.vertices[i].color);
            REQUIRE(std::abs(b.vertices[i].normal.LengthSquared()-1) < 1e-5f);
            Color before, translated;
            before.setPackedValueProperty(b.vertices[i].color);
            translated.setPackedValueProperty(c.vertices[i].color);
            REQUIRE(std::abs(before.getGProperty()-translated.getGProperty()) <= 1);
            REQUIRE(b.vertices[i].textureCoordinate.X == c.vertices[i].textureCoordinate.X);
            REQUIRE(b.vertices[i].textureCoordinate.Y == c.vertices[i].textureCoordinate.Y);
            changedColors += b.vertices[i].color != a.vertices[i].color;
        }
        REQUIRE(changedColors > 0);
        REQUIRE(changedPositions > 0);
        const float repeats=6.283185307f*2/1.6f;
        for (std::size_t i=0;i<b.indices.size();i+=3)
        {
            const float u=b.vertices[b.indices[i]].textureCoordinate.X;
            const float v=b.vertices[b.indices[i+1]].textureCoordinate.X;
            const float w=b.vertices[b.indices[i+2]].textureCoordinate.X;
            REQUIRE(std::isfinite(u) && std::isfinite(v) && std::isfinite(w));
            REQUIRE(std::max({u,v,w})-std::min({u,v,w}) <= repeats*0.501f);
        }
    }
}

TEST_CASE("shaped upholstery preserves texture scale and smooth outward normals")
{
    for (bool positive : {false,true})
    {
        MeshBuilder builder;
        TrainSim::Render::addShapedSeatFabric(builder,0,0.37f,0.69f,1.29f,3,positive,Color::White);
        MeshBuilder translated;
        TrainSim::Render::addShapedSeatFabric(translated,10,10.37f,4.69f,5.29f,103,positive,Color::White);
        const auto mesh = builder.take();
        REQUIRE(mesh.indices.size() == 144);
        float maximumDepth = 0;
        for (const auto& v : mesh.vertices)
        {
            const float depth = (v.position.Z-3)*(positive ? 1 : -1);
            REQUIRE(depth >= -1e-6f && depth <= 0.0181f);
            maximumDepth = std::max(maximumDepth,depth);
            REQUIRE(v.normal.Z*(positive ? 1 : -1) > 0.9f);
            REQUIRE(std::abs(v.normal.LengthSquared()-1) < 1e-5f);
            REQUIRE(std::abs(v.textureCoordinate.X-v.position.X/0.16f) < 1e-5f);
            REQUIRE(std::abs(v.textureCoordinate.Y-(v.position.Y-0.69f)/0.16f) < 1e-5f);
            if (v.position.X == 0 || std::abs(v.position.X-0.37f) < 1e-6f)
                REQUIRE(std::abs(depth) < 1e-6f);
        }
        REQUIRE(maximumDepth > 0.0179f);
        for (std::size_t i=0;i<mesh.vertices.size();++i)
            REQUIRE((mesh.vertices[i].textureCoordinate-translated.data().vertices[i].textureCoordinate)
                    .LengthSquared() < 1e-9f);
        for (std::size_t i = 0; i < mesh.indices.size(); i += 3)
        {
            const auto& a = mesh.vertices[mesh.indices[i]];
            const auto& b = mesh.vertices[mesh.indices[i+1]];
            const auto& c = mesh.vertices[mesh.indices[i+2]];
            REQUIRE(Vector3::Dot(Vector3::Cross(b.position-a.position,c.position-a.position),
                                 a.normal) < 0); // CNA clockwise front-face convention.
        }
    }
}

TEST_CASE("draped headrest covers clear the crown and wrap over its top in both directions")
{
    for (bool forward : {false,true})
    {
        MeshBuilder builder;
        TrainSim::Render::addSeatHeadCover(builder,1,3,forward,Color::White);
        const auto mesh = builder.take();
        REQUIRE(!mesh.empty());
        bool wrapsBack=false, reachesTop=false;
        for (const auto& v : mesh.vertices)
        {
            const float frontDistance = (v.position.Z-3.05f)*(forward ? -1.0f : 1.0f);
            REQUIRE(frontDistance > -0.028f);
            REQUIRE(frontDistance < 0.095f);
            REQUIRE(v.position.X >= 0.8099f && v.position.X <= 1.1901f);
            REQUIRE(v.position.Y >= 1.3099f && v.position.Y <= 1.6151f);
            const float dx=(v.position.X-1)/0.222f;
            const float dy=(v.position.Y-1.505f)/0.105f;
            const float dz=frontDistance/0.082f;
            REQUIRE(dx*dx+dy*dy+dz*dz>1.01f);
            // Backrest box ends at 1.56 m; its rounded corners are lower.
            REQUIRE(v.position.Y>1.56f || std::abs(frontDistance)>0.0501f);
            REQUIRE(std::isfinite(v.normal.X) && std::isfinite(v.normal.Y));
            wrapsBack |= frontDistance<0;
            reachesTop |= v.position.Y>1.614f;
        }
        REQUIRE(wrapsBack && reachesTop);
        REQUIRE(mesh.indices.size()/3<=440);
        for (std::size_t i=0;i<mesh.indices.size();i+=3)
        {
            const auto a=mesh.vertices[mesh.indices[i]].position;
            const auto b=mesh.vertices[mesh.indices[i+1]].position;
            const auto c=mesh.vertices[mesh.indices[i+2]].position;
            REQUIRE(Vector3::Cross(b-a,c-a).LengthSquared()>1e-14f);
            const auto p=(a+b+c)/3.0f;
            const float dx=(p.X-1)/0.222f,dy=(p.Y-1.505f)/0.105f;
            const float dz=(p.Z-3.05f)/0.082f;
            REQUIRE(dx*dx+dy*dy+dz*dz>1.001f);
        }
    }
}

TEST_CASE("car role pictograms and labels fit their separated sign areas")
{
    using namespace TrainSim::Render;
    using Microsoft::Xna::Framework::Matrix;
    for (int car=0;car<6;++car)
    {
        MeshBuilder icon;
        addCarRoleIcon(icon,car,Matrix::getIdentityProperty(),Color(255,255,255,255));
        REQUIRE(!icon.data().indices.empty());
        REQUIRE(icon.data().indices.size()/3<=150);
        for (const auto& v : icon.data().vertices)
        {
            REQUIRE(std::abs(v.position.X)<0.075f);
            REQUIRE(std::abs(v.position.Y)<0.075f);
            REQUIRE(std::abs(v.position.Z)<1e-6f);
            REQUIRE(std::abs(v.normal.Length()-1)<0.001f);
        }
        MeshBuilder label;
        addPassengerScreenText(label,PassengerCarRoleLabels[car],2.190f,0.064f,false,
                               Color(255,255,255,255),0.0038f,-0.85f,1.30f);
        for (const auto& v : label.data().vertices)
        {
            REQUIRE(v.position.X>=-0.851f && v.position.X<=0.451f);
            REQUIRE(v.position.Y>2.145f && v.position.Y<2.32f);
        }
    }
    MeshBuilder invalid;
    addCarRoleIcon(invalid,-1,Matrix::getIdentityProperty(),Color(255,255,255,255));
    REQUIRE(invalid.data().indices.empty());
}

TEST_CASE("family activity book remains on the table and clear of cup recesses")
{
    MeshBuilder book;
    TrainSim::Render::addFamilyActivityBook(book,10.25f);
    REQUIRE(!book.data().indices.empty());
    REQUIRE(book.data().indices.size()/3<1000);
    for (const auto& v : book.data().vertices)
    {
        REQUIRE(v.position.X>=0.739f && v.position.X<=1.215f);
        REQUIRE(v.position.Y>=0.860f && v.position.Y<=0.873f);
        REQUIRE(std::abs(v.position.Z-10.25f)<=0.116f);
        REQUIRE(std::isfinite(v.normal.X));
    }
}

TEST_CASE("vestibule service panels preserve clearance and separate upper and lower skins")
{
    for (float side : {-1.0f,1.0f})
        for (float face : {-1.0f,1.0f})
        {
            MeshBuilder builder;
            TrainSim::Render::addVestibuleServicePanels(builder,side,3,face);
            REQUIRE(builder.data().indices.size()/3<=200);
            bool lower=false,upper=false,reveal=false;
            for (const auto& v : builder.data().vertices)
            {
                REQUIRE(v.position.X*side>=0.6579f && v.position.X*side<=1.4121f);
                REQUIRE(v.position.Y>=0.1679f && v.position.Y<=2.2921f);
                REQUIRE((v.position.Z-3)*face<=0.0001f);
                REQUIRE((v.position.Z-3)*face>=-0.0211f);
                REQUIRE(std::abs(v.normal.Length()-1)<0.001f);
                REQUIRE(std::abs(v.textureCoordinate.Y-v.position.Y/0.23f)<1e-5f);
                Color c; c.setPackedValueProperty(v.color);
                if (c.getRProperty()>=70) // Rounded-box caps retain their authored shade factors.
                {
                    REQUIRE(v.position.Y<=1.0901f || v.position.Y>=1.1019f);
                    lower |= v.position.Y<=1.0901f;
                    upper |= v.position.Y>=1.1019f;
                }
                else reveal=true;
            }
            REQUIRE(lower && upper && reveal);
        }
}

TEST_CASE("gangway bellows form a closed corrugated skin outside the walking envelope")
{
    MeshBuilder builder;
    TrainSim::Render::addFlexibleGangwayBellows(builder,-42);
    const auto& mesh=builder.data();
    REQUIRE(mesh.indices.size()/3==2732);
    using Key=std::tuple<int,int,int>;
    const auto key=[](const Vector3& p) { return Key{
        static_cast<int>(std::lround(p.X*100000)),static_cast<int>(std::lround(p.Y*100000)),
        static_cast<int>(std::lround(p.Z*100000))}; };
    std::map<std::pair<Key,Key>,int> edges;
    float minimumSide=1,maximumSide=0;
    for (const auto& v : mesh.vertices)
    {
        REQUIRE(std::abs(v.position.X)<=0.6271f);
        REQUIRE(v.position.Y>=0.1049f && v.position.Y<=2.4161f);
        REQUIRE(std::abs(v.position.Z+42)<=0.3101f);
        REQUIRE(std::abs(v.normal.Length()-1)<0.001f);
        REQUIRE(std::isfinite(v.textureCoordinate.X) && std::isfinite(v.textureCoordinate.Y));
        if (v.position.Y<2.2f)
        {
            REQUIRE(std::abs(v.position.X)>=0.5649f);
            minimumSide=std::min(minimumSide,std::abs(v.position.X));
            maximumSide=std::max(maximumSide,std::abs(v.position.X));
        }
    }
    REQUIRE(maximumSide-minimumSide>0.060f);
    for (std::size_t i=0;i<mesh.indices.size();i+=3)
    {
        const auto& a=mesh.vertices[mesh.indices[i]];
        const auto& b=mesh.vertices[mesh.indices[i+1]];
        const auto& c=mesh.vertices[mesh.indices[i+2]];
        const auto cross=Vector3::Cross(b.position-a.position,c.position-a.position);
        REQUIRE(cross.LengthSquared()>1e-13f);
        REQUIRE(Vector3::Dot(cross,a.normal+b.normal+c.normal)<0);
        const std::array<Key,3> points={key(a.position),key(b.position),key(c.position)};
        for (int e=0;e<3;++e)
        {
            auto lo=points[e],hi=points[(e+1)%3];
            if (hi<lo) std::swap(lo,hi);
            ++edges[{lo,hi}];
        }
    }
    for (const auto& [edge,count] : edges) { (void)edge; REQUIRE(count==2); }
}

TEST_CASE("door controls are mounted within the jamb with readable two-sided button faces")
{
    for (float side : {-1.0f,1.0f})
    {
        MeshBuilder housing;
        TrainSim::Render::addPassengerDoorControlHousing(housing,side,-45.28f);
        REQUIRE(housing.data().indices.size()/3<1600);
        for (const auto& v : housing.data().vertices)
        {
            REQUIRE(std::abs(v.position.X-side*1.45f)<0.071f);
            REQUIRE(std::abs(v.position.Z+45.28f)<=0.0551f);
            REQUIRE(v.position.Y>=1.2349f && v.position.Y<=1.4751f);
            REQUIRE(std::abs(v.normal.Length()-1)<0.001f);
            Color color; color.setPackedValueProperty(v.color);
            if (color.getRProperty()==233 && v.position.X*side<1.45f)
                REQUIRE(v.position.X*side<1.396f); // Lettering clears the rounded window liner.
        }
    }
    MeshBuilder button;
    TrainSim::Render::addPassengerDoorButtonFaces(button);
    REQUIRE(button.data().indices.size()/3==48);
    bool inside=false,outside=false;
    for (const auto& v : button.data().vertices)
    {
        REQUIRE(std::abs(std::abs(v.position.X)-0.069f)<1e-6f);
        REQUIRE(v.position.Y*v.position.Y+v.position.Z*v.position.Z<=0.03201f*0.03201f);
        REQUIRE(v.normal.X*v.position.X>0.068f);
        inside |= v.normal.X<0;
        outside |= v.normal.X>0;
    }
    REQUIRE(inside && outside);
}

TEST_CASE("passenger door leaf has a sealed real aperture rather than an opaque window patch")
{
    MeshBuilder leaf;
    TrainSim::Render::addPassengerDoorLeaf(leaf);
    REQUIRE(leaf.data().indices.size()/3<350);
    for (const auto& v : leaf.data().vertices)
    {
        REQUIRE(std::abs(v.position.X)<=0.0551f);
        REQUIRE(std::abs(v.position.Z)<=0.3401f);
        REQUIRE(v.position.Y>=-1.1101f && v.position.Y<=1.1101f);
        REQUIRE(std::abs(v.normal.Length()-1)<0.001f);
    }
    // Rays through a 3x3 grid inside the clear pane must not hit any opaque YZ face.
    for (float y : {-0.10f,0.19f,0.48f})
        for (float z : {-0.12f,0.0f,0.12f})
            for (std::size_t i=0;i<leaf.data().indices.size();i+=3)
            {
                const auto& a=leaf.data().vertices[leaf.data().indices[i]].position;
                const auto& b=leaf.data().vertices[leaf.data().indices[i+1]].position;
                const auto& c=leaf.data().vertices[leaf.data().indices[i+2]].position;
                const float area=(b.Y-a.Y)*(c.Z-a.Z)-(b.Z-a.Z)*(c.Y-a.Y);
                if (std::abs(area)<1e-8f) continue;
                const float u=((y-a.Y)*(c.Z-a.Z)-(z-a.Z)*(c.Y-a.Y))/area;
                const float v=((b.Y-a.Y)*(z-a.Z)-(b.Z-a.Z)*(y-a.Y))/area;
                REQUIRE(!(u>=0 && v>=0 && u+v<=1));
            }
}

TEST_CASE("door pane follows its moving leaf with transparent two-sided bounded geometry")
{
    for (float centreZ : {-44.83f,-44.17f,-45.46f})
    {
        MeshBuilder pane;
        TrainSim::Render::addPassengerDoorPane(pane,1.455f,1.17f,centreZ);
        REQUIRE(pane.data().indices.size()/3==40);
        bool positive=false,negative=false;
        for (const auto& v : pane.data().vertices)
        {
            REQUIRE(v.position.X==1.455f);
            REQUIRE(std::abs(v.position.Z-centreZ)<=0.2051f);
            REQUIRE(v.position.Y>=0.9349f && v.position.Y<=1.7851f);
            Color color; color.setPackedValueProperty(v.color);
            REQUIRE(color.getAProperty()==22);
            positive |= v.normal.X>0.999f;
            negative |= v.normal.X<-0.999f;
        }
        REQUIRE(positive && negative);
    }
}

TEST_CASE("passenger screen atlas has antialiasing and proportional mixed-case glyphs")
{
    const auto image=TrainSim::Render::makePassengerDisplayFontImage();
    REQUIRE(image.width==512 && image.height==256);
    REQUIRE(image.pixels.size()==512*256);
    bool transparent=false,antialiased=false,opaque=false;
    for (const auto& color : image.pixels)
    {
        const auto alpha=color.getAProperty();
        transparent |= alpha==0;
        antialiased |= alpha>0 && alpha<255;
        opaque |= alpha==255;
        REQUIRE(color.getRProperty()==255 && color.getGProperty()==255 && color.getBProperty()==255);
    }
    REQUIRE(transparent && antialiased && opaque);
    using namespace TrainSim::Render::DisplayFontData;
    REQUIRE(Glyphs['W'-32].advance64>Glyphs['i'-32].advance64*2);
    REQUIRE(Glyphs['A'-32].height>Glyphs['a'-32].height);
    for (const auto& g : Glyphs)
    {
        REQUIRE(g.x>=0 && g.y>=0 && g.x+g.width<=512 && g.y+g.height<=256);
        REQUIRE(g.advance64>0);
    }
}

TEST_CASE("passenger typography fits both display faces without mirrored letters or overflow")
{
    for (bool mirrored : {false,true})
        for (const std::string_view text : {"Next: Westhaven Central", "To Alderwick Central",
                                           "120 km/h   Arriving in 5 min", "Doors open: left",
                                           "An exceptionally long following station name"})
        {
            MeshBuilder mesh;
            TrainSim::Render::addPassengerScreenText(mesh,text,2.652f,3,mirrored,Color::White,0.005f);
            REQUIRE(!mesh.data().empty());
            REQUIRE(mesh.data().indices.size()/3<=text.size()*2);
            for (const auto& v : mesh.data().vertices)
            {
                REQUIRE(std::abs(v.position.X)<0.90f);
                REQUIRE(v.position.Y>2.60f && v.position.Y<2.778f);
                REQUIRE(v.normal.Z*(mirrored ? -1.0f : 1.0f)>0.999f);
                REQUIRE(v.textureCoordinate.X>=0 && v.textureCoordinate.X<=1);
                REQUIRE(v.textureCoordinate.Y>=0 && v.textureCoordinate.Y<=1);
            }
        }
}

TEST_CASE("passenger display housing surrounds a recessed face and meets the ceiling")
{
    MeshBuilder builder;
    TrainSim::Render::addPassengerDisplayHousing(builder,3);
    REQUIRE(builder.data().indices.size()/3<1000);
    bool ceiling=false,front=false,rear=false;
    for (const auto& v : builder.data().vertices)
    {
        REQUIRE(std::abs(v.position.X)<=0.9851f);
        REQUIRE(v.position.Y>=2.3199f && v.position.Y<=2.9151f);
        REQUIRE(std::abs(v.position.Z-3)<=0.1001f);
        REQUIRE(std::abs(v.normal.Length()-1)<0.001f);
        ceiling |= v.position.Y>=2.914f;
        front |= v.position.Z>3.099f;
        rear |= v.position.Z<2.901f;
    }
    REQUIRE(ceiling && front && rear);
}

TEST_CASE("suspended handles have smooth closed grips and bounded ceiling attachments")
{
    MeshBuilder metal,grip;
    TrainSim::Render::addSuspendedPassengerHandle(metal,grip,0.38f,-48.5f);
    REQUIRE(grip.data().indices.size()/3==384);
    REQUIRE(!metal.data().empty());
    for (const auto* mesh : {&metal.data(),&grip.data()})
        for (const auto& v : mesh->vertices)
        {
            REQUIRE(std::abs(v.position.X-0.38f)<=0.0851f);
            REQUIRE(std::abs(v.position.Z+48.5f)<=0.0351f);
            REQUIRE(v.position.Y>=2.3359f && v.position.Y<=2.9151f);
            const auto n=v.normal;
            REQUIRE(std::abs(n.X*n.X+n.Y*n.Y+n.Z*n.Z-1)<0.001f);
        }
    float attachmentTop=0;
    for (const auto& v : metal.data().vertices) attachmentTop=std::max(attachmentTop,v.position.Y);
    REQUIRE(attachmentTop>=2.914f); // Meets the actual ceiling skin, not a floating plate.
    // CNA/XNA uses clockwise indices; geometric cross products oppose shading normals.
    const auto& data=grip.data();
    for (std::size_t i=0;i<data.indices.size();i+=3)
    {
        const auto& a=data.vertices[data.indices[i]];
        const auto& b=data.vertices[data.indices[i+1]];
        const auto& c=data.vertices[data.indices[i+2]];
        const auto u=b.position-a.position,v=c.position-a.position;
        const Vector3 cross{u.Y*v.Z-u.Z*v.Y,u.Z*v.X-u.X*v.Z,u.X*v.Y-u.Y*v.X};
        REQUIRE(cross.X*a.normal.X+cross.Y*a.normal.Y+cross.Z*a.normal.Z<0);
    }
}

TEST_CASE("carpet bindings close the perimeter and stay flush within each floor span")
{
    MeshBuilder builder;
    const int count=TrainSim::Render::addCarpetBindings(builder,-10,8);
    REQUIRE(count==16);
    REQUIRE(builder.data().indices.size()==static_cast<std::size_t>(count)*6);
    bool firstEnd=false,lastEnd=false;
    for (const auto& v : builder.data().vertices)
    {
        REQUIRE(std::abs(v.position.X)<=0.3901f);
        REQUIRE(v.position.Z>=-10 && v.position.Z<=8);
        REQUIRE(v.position.Y==0.099f);
        REQUIRE(v.normal.Y>0.999f);
        REQUIRE(std::abs(v.textureCoordinate.X-v.position.X/0.85f)<1e-5f);
        REQUIRE(std::abs(v.textureCoordinate.Y-v.position.Z/0.85f)<1e-5f);
        firstEnd |= v.position.Z==-10 && std::abs(v.position.X)<0.385f;
        lastEnd |= v.position.Z==8 && std::abs(v.position.X)<0.385f;
    }
    REQUIRE(firstEnd && lastEnd);
    MeshBuilder shortSpan;
    REQUIRE(TrainSim::Render::addCarpetBindings(shortSpan,1,1.2f)==4);
    MeshBuilder empty;
    REQUIRE(TrainSim::Render::addCarpetBindings(empty,1,1.05f)==0);
    REQUIRE(empty.data().empty());
}

TEST_CASE("carpet wear follows the walked centre and preserves protected edges and UV scale")
{
    MeshBuilder builder;
    TrainSim::Render::addWornCarpetRunner(builder,-10,8);
    REQUIRE(!builder.data().empty());
    REQUIRE(builder.data().indices.size()/3<=512);
    bool worn=false,protectedEdge=false;
    for (const auto& v : builder.data().vertices)
    {
        REQUIRE(std::abs(v.position.X)<=0.3901f);
        REQUIRE(v.position.Z>=-10 && v.position.Z<=8);
        REQUIRE(v.position.Y==0.098f);
        REQUIRE(v.normal.Y>0.999f);
        REQUIRE(std::abs(v.textureCoordinate.X-v.position.X/0.85f)<1e-5f);
        REQUIRE(std::abs(v.textureCoordinate.Y-v.position.Z/0.85f)<1e-5f);
        Color c; c.setPackedValueProperty(v.color);
        REQUIRE(c.getAProperty()==255);
        REQUIRE(c.getRProperty()>=50 && c.getRProperty()<=64);
        if (std::abs(v.position.X)>0.34f)
        {
            REQUIRE(c.getRProperty()==50);
            protectedEdge=true;
        }
        if (std::abs(v.position.X)<0.01f)
        {
            REQUIRE(c.getRProperty()>=60);
            worn=true;
        }
    }
    REQUIRE(worn && protectedEdge);
    MeshBuilder empty;
    TrainSim::Render::addWornCarpetRunner(empty,1,1.05f);
    REQUIRE(empty.data().empty());
}

TEST_CASE("contact shadows have transparent boundaries and upward nondegenerate faces")
{
    MeshBuilder builder;
    TrainSim::Render::addSoftContactShadow(builder,2,0.104f,4,0.16f,0.20f,105);
    const auto& mesh=builder.data();
    REQUIRE(mesh.indices.size()==144);
    bool dark=false,clear=false;
    for (const auto& v : mesh.vertices)
    {
        const float dx=(v.position.X-2)/0.16f,dz=(v.position.Z-4)/0.20f;
        REQUIRE(dx*dx+dz*dz<1.0001f);
        REQUIRE(v.normal.Y>0.999f);
        Color c; c.setPackedValueProperty(v.color);
        REQUIRE(c.getAProperty()==0 || c.getAProperty()==105);
        if (dx*dx+dz*dz>0.99f) REQUIRE(c.getAProperty()==0);
        dark |= c.getAProperty()==105;
        clear |= c.getAProperty()==0;
    }
    REQUIRE(dark && clear);
    for (std::size_t i=0;i<mesh.indices.size();i+=3)
    {
        const auto a=mesh.vertices[mesh.indices[i]].position;
        const auto b=mesh.vertices[mesh.indices[i+1]].position;
        const auto c=mesh.vertices[mesh.indices[i+2]].position;
        REQUIRE(Vector3::Cross(b-a,c-a).LengthSquared()>1e-12f);
    }
}

TEST_CASE("ceiling light modules have bounded housings and downward graded diffuser faces")
{
    MeshBuilder housing,emitters;
    TrainSim::Render::addCeilingLightModule(housing,emitters,4,5.38f);
    REQUIRE(housing.data().indices.size()==180);
    REQUIRE(emitters.data().indices.size()==54);
    for (const auto* mesh : {&housing.data(),&emitters.data()})
        for (const auto& v : mesh->vertices)
        {
            REQUIRE(std::abs(v.position.X)<=0.1151f);
            REQUIRE(v.position.Y>=2.8149f && v.position.Y<=2.9131f);
            REQUIRE(v.position.Z>=3.9999f && v.position.Z<=5.3801f);
        }
    bool centre=false,edge=false;
    for (const auto& v : emitters.data().vertices)
    {
        REQUIRE(v.normal.Y<-0.8f);
        Color c; c.setPackedValueProperty(v.color);
        centre |= c.getRProperty()==246;
        edge |= c.getRProperty()==219;
    }
    REQUIRE(centre && edge);
    MeshBuilder emptyHousing,emptyEmitters;
    TrainSim::Render::addCeilingLightModule(emptyHousing,emptyEmitters,1,1.1f);
    REQUIRE(emptyHousing.data().empty() && emptyEmitters.data().empty());
}

TEST_CASE("window reflections fade to zero at every perimeter and face the cabin")
{
    for (float x : {-1.439f,1.439f})
    {
        MeshBuilder mesh;
        TrainSim::Render::addSoftWindowReflection(mesh,x,2,4,0.2f,0.5f,16);
        REQUIRE(mesh.data().indices.size()==96);
        bool peak=false;
        for (const auto& v : mesh.data().vertices)
        {
            Color color; color.setPackedValueProperty(v.color);
            REQUIRE(v.position.X==x);
            REQUIRE(v.normal.X*x<0);
            REQUIRE(color.getAProperty()<=16);
            if (std::abs(v.position.Y-2)>0.199f || std::abs(v.position.Z-4)>0.499f)
                REQUIRE(color.getAProperty()==0);
            peak |= color.getAProperty()==16;
        }
        REQUIRE(peak);
    }
}

TEST_CASE("mullion caps mirror within the structural pier and keep clear glazing open")
{
    for (float side : {-1.0f,1.0f})
    {
        MeshBuilder mesh;
        TrainSim::Render::addPassengerMullionCap(mesh,side,12,0.12f);
        REQUIRE(!mesh.data().empty());
        REQUIRE(mesh.data().indices.size()/3<120);
        for (const auto& v : mesh.data().vertices)
        {
            REQUIRE(v.position.X*side>=1.3929f);
            REQUIRE(v.position.X*side<=1.4191f);
            REQUIRE(std::abs(v.position.Z-12)<0.117f);
            REQUIRE(v.position.Y>=0.8449f && v.position.Y<=2.3551f);
            REQUIRE(std::abs(v.normal.LengthSquared()-1)<1e-5f);
            REQUIRE(std::isfinite(v.textureCoordinate.X));
            REQUIRE(std::isfinite(v.textureCoordinate.Y));
        }
    }
}

TEST_CASE("laminate table edges and inset trays stay within existing table collision")
{
    for (float x : {-0.92f,0.92f})
    {
        MeshBuilder mesh;
        TrainSim::Render::addPassengerTableTop(mesh,x,12,Color::White);
        REQUIRE(mesh.data().indices.size()/3<500);
        bool bevel=false;
        for (const auto& v : mesh.data().vertices)
        {
            REQUIRE(std::abs(v.position.X-x)<=0.4401f);
            REQUIRE(std::abs(v.position.Z-12)<=0.3801f);
            REQUIRE(v.position.Y>=0.7749f && v.position.Y<=0.8611f);
            if (v.position.Y<0.796f)
            {
                REQUIRE(std::abs(v.position.X-x)<=0.4001f);
                REQUIRE(std::abs(v.position.Z-12)<=0.3401f);
            }
            REQUIRE(std::abs(v.normal.LengthSquared()-1)<1e-5f);
            REQUIRE(std::isfinite(v.textureCoordinate.X));
            REQUIRE(std::isfinite(v.textureCoordinate.Y));
            bevel |= v.normal.Y>0.1f && v.normal.Y<0.95f;
        }
        REQUIRE(bevel);
    }
}

TEST_CASE("table cup inserts have open tops and floors below the laminate")
{
    for (float x : {-0.92f,0.92f})
    {
        MeshBuilder builder;
        TrainSim::Render::addTableSurfaceWithCupRecesses(builder,x,12,Color::White);
        const auto& mesh=builder.data();
        const float cupX=x+(x>0 ? 0.06f : -0.06f);
        bool floor=false;
        for (const auto& v : mesh.vertices)
            floor |= std::abs(v.position.Y-0.848f)<1e-6f && v.normal.Y>0.999f;
        REQUIRE(floor);
        const auto cross2=[](const Vector3& a,const Vector3& b,const Vector3& p) {
            return (b.X-a.X)*(p.Z-a.Z)-(b.Z-a.Z)*(p.X-a.X);
        };
        for (std::size_t i=0;i<mesh.indices.size();i+=3)
        {
            const auto a=mesh.vertices[mesh.indices[i]].position;
            const auto b=mesh.vertices[mesh.indices[i+1]].position;
            const auto c=mesh.vertices[mesh.indices[i+2]].position;
            REQUIRE(Vector3::Cross(b-a,c-a).LengthSquared()>1e-14f);
            if (a.Y<0.859f || b.Y<0.859f || c.Y<0.859f) continue;
            for (float cupZ : {-0.19f,0.19f})
                for (float dx : {-0.025f,0.0f,0.025f})
                    for (float dz : {-0.025f,0.0f,0.025f})
                    {
                        const Vector3 p{cupX+dx,0,12+cupZ+dz};
                        const float ab=cross2(a,b,p),bc=cross2(b,c,p),ca=cross2(c,a,p);
                        const bool inside=(ab>=0 && bc>=0 && ca>=0) || (ab<=0 && bc<=0 && ca<=0);
                        REQUIRE(!inside);
                    }
        }
    }
}

TEST_CASE("padded armrests retain their collision envelope and curved upper normals")
{
    for (float x : {-0.34f,0.34f})
    {
        MeshBuilder frame,pad;
        TrainSim::Render::addPassengerArmrest(frame,pad,x,12);
        REQUIRE(!frame.data().empty() && !pad.data().empty());
        REQUIRE((frame.data().indices.size()+pad.data().indices.size())/3<300);
        bool roll=false;
        for (const auto* mesh : {&frame.data(),&pad.data()})
            for (const auto& v : mesh->vertices)
            {
                REQUIRE(std::abs(v.position.X-x)<=0.0451f);
                REQUIRE(std::abs(v.position.Z-12)<=0.2701f);
                REQUIRE(v.position.Y>=0.8799f && v.position.Y<=0.9401f);
                REQUIRE(std::abs(v.normal.LengthSquared()-1)<1e-5f);
                REQUIRE(std::isfinite(v.textureCoordinate.X));
                REQUIRE(std::isfinite(v.textureCoordinate.Y));
                roll |= v.normal.Y>0.70f && v.normal.Y<0.72f;
            }
        REQUIRE(roll);
    }
}

TEST_CASE("moulded seat rear shells mirror depth without exceeding the seat envelope")
{
    for (float direction : {-1.0f,1.0f})
    {
        MeshBuilder builder;
        TrainSim::Render::addSeatRearShell(builder,1,3,direction,Color::White);
        const auto mesh = builder.take();
        REQUIRE(!mesh.empty());
        for (const auto& v : mesh.vertices)
        {
            REQUIRE(v.position.X >= 0.7929f);
            REQUIRE(v.position.X <= 1.2071f);
            REQUIRE(v.position.Y >= 0.6799f);
            REQUIRE(v.position.Y <= 1.5201f);
            const float rearDepth=(v.position.Z-3)*direction;
            REQUIRE(rearDepth >= 0.0239f);
            REQUIRE(rearDepth <= 0.0551f);
            // Real seat rear reference is 64 mm from the crown centre: no face may enter
            // the 82 mm crown, including the panel's lower-depth front-facing surface.
            REQUIRE(rearDepth+0.064f > 0.082f);
            // The furthest rear shell remains inside the authoritative +/-480 mm seat bay.
            REQUIRE(0.394f+rearDepth < 0.48f);
            REQUIRE(std::abs(v.normal.LengthSquared()-1) < 1e-5f);
        }
    }
}

TEST_CASE("seat shell bevels face outward and polymer grain is translation invariant")
{
    for (float direction : {-1.0f,1.0f})
    {
        MeshBuilder base,moved;
        TrainSim::Render::addSeatRearShell(base,0,0,direction,Color::White);
        TrainSim::Render::addSeatRearShell(moved,12,60,direction,Color::White);
        REQUIRE(base.data().indices==moved.data().indices);
        int bevelVertices=0;
        for (std::size_t i=0;i<base.data().vertices.size();++i)
        {
            const auto& v=base.data().vertices[i];
            const auto uv=moved.data().vertices[i].textureCoordinate;
            REQUIRE(std::isfinite(v.textureCoordinate.X));
            REQUIRE(std::isfinite(v.textureCoordinate.Y));
            REQUIRE(std::abs(v.textureCoordinate.X-uv.X)<0.0001f);
            REQUIRE(std::abs(v.textureCoordinate.Y-uv.Y)<0.0001f);
            REQUIRE(std::abs(v.textureCoordinate.X-(v.position.X+0.207f)/0.12f)<1e-5f);
            REQUIRE(std::abs(v.textureCoordinate.Y-(v.position.Y-0.68f)/0.12f)<1e-5f);
            if (std::abs(v.normal.Z)>0.1f && std::abs(v.normal.Z)<0.95f)
            {
                REQUIRE(v.normal.Z*direction>0);
                ++bevelVertices;
            }
        }
        REQUIRE(bevelVertices>=64);
        REQUIRE(base.data().indices.size()/3<600);
    }
}

TEST_CASE("luggage rack spans stay above passengers and respect compartment endpoints")
{
    for (float side : {-1.0f,1.0f})
    {
        MeshBuilder builder;
        TrainSim::Render::addLuggageRackSpan(builder,side,3,6);
        const auto mesh = builder.take();
        REQUIRE(!mesh.empty());
        REQUIRE(mesh.indices.size()/3<700);
        for (const auto& v : mesh.vertices)
        {
            REQUIRE(v.position.X*side >= 0.8499f);
            REQUIRE(v.position.X*side <= 1.2351f);
            REQUIRE(v.position.Y >= 2.2099f);
            REQUIRE(v.position.Y <= 2.2481f);
            REQUIRE(v.position.Z >= 3);
            REQUIRE(v.position.Z <= 6);
        }
        TrainSim::Render::addLuggageRackSpan(builder,side,6,3);
        REQUIRE(builder.take().empty());
    }
}

TEST_CASE("rack support blades meet the shelf and attach to finished mullions")
{
    for (float side : {-1.0f,1.0f})
    {
        MeshBuilder builder;
        TrainSim::Render::addLuggageRackBracket(builder,side,5);
        bool shelfContact=false,wallContact=false;
        REQUIRE(builder.data().indices.size()/3<250);
        for (const auto& v : builder.data().vertices)
        {
            REQUIRE(v.position.X*side>=0.8599f && v.position.X*side<=1.4021f);
            REQUIRE(v.position.Y>=2.0799f && v.position.Y<=2.2601f);
            REQUIRE(std::abs(v.position.Z-5)<=0.0291f);
            REQUIRE(std::abs(v.normal.Length()-1)<0.001f);
            shelfContact |= v.position.X*side<0.87f && std::abs(v.position.Y-2.214f)<1e-5f;
            wallContact |= v.position.X*side>1.394f;
        }
        REQUIRE(shelfContact && wallContact);
        const auto& data=builder.data();
        for (std::size_t i=0;i<data.indices.size();i+=3)
        {
            const auto& a=data.vertices[data.indices[i]];
            const auto& b=data.vertices[data.indices[i+1]];
            const auto& c=data.vertices[data.indices[i+2]];
            REQUIRE(Vector3::Dot(Vector3::Cross(b.position-a.position,c.position-a.position),
                                 a.normal+b.normal+c.normal)<0);
        }
    }
}

TEST_CASE("ceiling coves face the cabin and join wall and ceiling within their envelope")
{
    for (float side : {-1.0f,1.0f})
    {
        MeshBuilder builder;
        TrainSim::Render::addCeilingCove(builder,side,-10,10,Color::White);
        const auto mesh = builder.take();
        REQUIRE(mesh.indices.size() == 48);
        REQUIRE(mesh.vertices.size() == 32);
        for (const auto& v : mesh.vertices)
        {
            REQUIRE(v.normal.X*side < 0);
            REQUIRE(v.normal.Y < 0);
            REQUIRE(std::abs(v.normal.LengthSquared()-1) < 1e-5f);
            REQUIRE(v.position.X*side >= 1.1659f);
            REQUIRE(v.position.X*side <= 1.4161f);
            REQUIRE(v.position.Y >= 2.6639f);
            REQUIRE(v.position.Y <= 2.9141f);
        }
    }
}

TEST_CASE("avatar view culling keeps intersecting bounds and handles rotated cameras")
{
    using Microsoft::Xna::Framework::Matrix;
    using TrainSim::Render::sphereMayBeInView;
    const auto projection = Matrix::CreatePerspectiveFieldOfView(1.570796327f,1.0f,0.1f,100.0f);
    REQUIRE(sphereMayBeInView(projection,{0,0,-5},1));
    REQUIRE(!sphereMayBeInView(projection,{0,0,5},1));
    REQUIRE(!sphereMayBeInView(projection,{20,0,-5},1));
    REQUIRE(!sphereMayBeInView(projection,{0,20,-5},1));
    REQUIRE(sphereMayBeInView(projection,{5.5f,0,-5},1));
    REQUIRE(sphereMayBeInView(projection,{0,0,0},1));
    REQUIRE(sphereMayBeInView(projection,{0,0,-200},1)); // No far-plane rejection.
    const auto rotated = Matrix::CreateLookAt({10,2,3},{11,2,3},Vector3::Up)*projection;
    REQUIRE(sphereMayBeInView(rotated,{15,2,3},1));
    REQUIRE(!sphereMayBeInView(rotated,{5,2,3},1));
    REQUIRE(!sphereMayBeInView(rotated,{10,2,-10},1));
}

TEST_CASE("distant ridge variation actually changes between route chunks")
{
    for (int side = 0; side < 2; ++side)
        for (int layer = 0; layer < 2; ++layer)
        {
            std::array<bool,5> seen{};
            for (std::int64_t chunk = -20; chunk < 20; ++chunk)
            {
                const int variant = TrainSim::Render::ridgeVariant(chunk,side,layer);
                REQUIRE(variant >= 0 && variant < 5);
                REQUIRE(variant == TrainSim::Render::ridgeVariant(chunk,side,layer));
                seen[static_cast<std::size_t>(variant)] = true;
            }
            for (bool present : seen) REQUIRE(present);
        }
}

TEST_CASE("grass ribbons are bounded double-sided geometry with no box cap")
{
    MeshBuilder builder;
    const Vector3 root{2,3,4};
    TrainSim::Render::addGrassBlade(builder,root,0.5f,0.08f,0.7f,Color(70,110,50,255));
    const auto mesh = builder.take();
    REQUIRE(mesh.indices.size() == 18);
    REQUIRE(mesh.vertices.size() == 14);
    for (const auto& v : mesh.vertices)
    {
        REQUIRE(v.position.Y >= root.Y);
        REQUIRE(v.position.Y <= root.Y+0.5f);
        REQUIRE(std::hypot(v.position.X-root.X,v.position.Z-root.Z) < 0.16f);
        REQUIRE(std::abs(v.normal.LengthSquared()-1.0f) < 1e-5f);
    }
    REQUIRE((mesh.vertices[0].normal+mesh.vertices[4].normal).LengthSquared() < 1e-5f);
    TrainSim::Render::addGrassBlade(builder,root,0.0f,0.08f,0.0f,Color(70,110,50,255));
    REQUIRE(builder.take().empty());
}

TEST_CASE("smooth quads preserve distinct unit normals and clockwise indices")
{
    MeshBuilder builder;
    builder.addQuad(Vector3{0,0,0},Vector3{1,0,0},Vector3{1,1,0},Vector3{0,1,0},
        Color(200,210,220,255),Vector3{-1,0,2},Vector3{1,0,2},
        Vector3{1,0,2},Vector3::Zero);
    const auto mesh = builder.take();
    REQUIRE(mesh.vertices.size() == 4);
    REQUIRE(mesh.indices.size() == 6);
    for (const auto& vertex : mesh.vertices)
        REQUIRE(std::abs(vertex.normal.LengthSquared()-1.0f) < 1e-5f);
    REQUIRE(mesh.vertices[0].normal.X < 0.0f);
    REQUIRE(mesh.vertices[1].normal.X > 0.0f);
    REQUIRE(mesh.vertices[3].normal.Z > 0.99f);
    REQUIRE(mesh.indices[1] == 2);
    REQUIRE(mesh.indices[2] == 1);
}

TEST_CASE("daylight retains its absolute direction across route headings")
{
    const auto absolute = TrainSim::Render::daylightDirection(0.0);
    for (double heading : {-3.0,-0.5,0.0,0.5,3.0})
    {
        const auto local = TrainSim::Render::daylightDirection(heading);
        REQUIRE(std::abs(local.LengthSquared()-1.0f) < 1e-5f);
        REQUIRE(local.Y < 0.0f);
        REQUIRE(std::abs(local.X*std::cos(heading)+local.Z*std::sin(heading)-absolute.X) < 1e-5);
        REQUIRE(std::abs(-local.X*std::sin(heading)+local.Z*std::cos(heading)-absolute.Z) < 1e-5);
        REQUIRE(std::abs(local.Y-absolute.Y) < 1e-5f);
    }
}

TEST_CASE("exterior daylight separates skylight from sun without clipping the lit total")
{
    const auto day=TrainSim::Render::exteriorLighting(0);
    const auto tunnel=TrainSim::Render::exteriorLighting(1);
    REQUIRE(day.ambient.Z>day.ambient.X);
    REQUIRE(day.diffuse.X>day.diffuse.Z);
    REQUIRE((day.ambient+day.diffuse-Vector3{1.02f,1.01f,0.99f}).LengthSquared()<1e-10f);
    REQUIRE((tunnel.ambient-Vector3{0.40f,0.41f,0.43f}).LengthSquared()<1e-10f);
    REQUIRE((tunnel.diffuse-Vector3{0.12f,0.12f,0.12f}).LengthSquared()<1e-10f);
    const auto middle=TrainSim::Render::exteriorLighting(0.5f);
    REQUIRE((middle.ambient-(day.ambient+tunnel.ambient)*0.5f).LengthSquared()<1e-10f);
    REQUIRE((TrainSim::Render::exteriorLighting(-1).diffuse-day.diffuse).LengthSquared()==0);
    REQUIRE((TrainSim::Render::exteriorLighting(2).diffuse-tunnel.diffuse).LengthSquared()==0);
}

TEST_CASE("ground bounce fills only faces turned away from the sun")
{
    using namespace TrainSim::Render;
    const auto day=exteriorLighting(0);
    REQUIRE(day.bounce.X>day.bounce.Z);
    REQUIRE(exteriorLighting(1).bounce.LengthSquared()==0);
    for (double heading : {0.0,0.9,2.4,-1.7})
    {
        const Vector3 sun=daylightDirection(heading);
        const Vector3 bounce=groundBounceDirection(sun);
        // Terrain, roofs and every up-facing surface receive none of the fill.
        REQUIRE(Vector3::Dot(Vector3{0,1,0},-bounce)<0);
        const Vector3 away=Vector3::Normalize({sun.X,0,sun.Z});
        // A wall facing away from the sun is filled; a sun-facing wall is not.
        REQUIRE(Vector3::Dot(away,-bounce)>0.85f);
        REQUIRE(Vector3::Dot(-away,-bounce)<0);
        const float shaded=(day.ambient+day.bounce*Vector3::Dot(away,-bounce)).Length();
        const float lit=(day.ambient+day.diffuse*Vector3::Dot(-away,-sun)).Length();
        REQUIRE(shaded<lit);
    }
    // The uplight reaches canopy soffits but no wall or up-facing surface.
    REQUIRE(Vector3::Dot(Vector3{0,-1,0},-groundUplightDirection())>0.99f);
    REQUIRE(Vector3::Dot(Vector3{0,1,0},-groundUplightDirection())<0);
    REQUIRE(std::abs(Vector3::Dot(Vector3{1,0,0},-groundUplightDirection()))<1e-6f);
    REQUIRE(day.uplight.X>day.uplight.Z);
    REQUIRE(exteriorLighting(1).uplight.LengthSquared()==0);
}

TEST_CASE("cabin avatar lighting fades sunlight into overhead fill in tunnels")
{
    const auto day = TrainSim::Render::daylightDirection(0.3);
    const auto outside = TrainSim::Render::avatarCabinLighting(day,-1.0f);
    REQUIRE((outside.direction-day).LengthSquared() < 1e-6f);
    const auto inside = TrainSim::Render::avatarCabinLighting(day,2.0f);
    REQUIRE(std::abs(inside.direction.Y+1.0f) < 1e-6f);
    REQUIRE(inside.diffuse.X < outside.diffuse.X);
    REQUIRE(inside.diffuse.X > 0.1f);
    for (float t : {0.0f,0.25f,0.5f,0.75f,1.0f})
    {
        const auto light = TrainSim::Render::avatarCabinLighting(day,t);
        REQUIRE(std::abs(light.direction.LengthSquared()-1.0f) < 1e-5f);
        REQUIRE(light.direction.Y < 0.0f);
    }
}

TEST_CASE("clockwise procedural quads carry outward lighting normals")
{
    MeshBuilder builder;
    builder.addQuad(Vector3{-1.0f,-1.0f,0.0f}, Vector3{1.0f,-1.0f,0.0f},
                    Vector3{1.0f,1.0f,0.0f}, Vector3{-1.0f,1.0f,0.0f},
                    Color(120,180,220,255));
    const auto mesh = builder.take();

    REQUIRE(mesh.vertices.size() == 4);
    REQUIRE(mesh.indices.size() == 6);
    for (const auto& vertex : mesh.vertices)
    {
        REQUIRE(std::abs(vertex.normal.LengthSquared() - 1.0f) < 1.0e-5f);
        REQUIRE(vertex.normal.Z > 0.999f);
    }
}

TEST_CASE("gradient quads retain independent vertex colours")
{
    MeshBuilder builder;
    const std::array colors = {Color(10,20,30,255), Color(40,50,60,255),
                               Color(70,80,90,255), Color(100,110,120,255)};
    builder.addQuad(Vector3{-1.0f,-1.0f,0.0f}, Vector3{1.0f,-1.0f,0.0f},
                    Vector3{1.0f,1.0f,0.0f}, Vector3{-1.0f,1.0f,0.0f},
                    colors[0], colors[1], colors[2], colors[3]);
    const auto mesh = builder.take();
    REQUIRE(mesh.vertices.size() == colors.size());
    for (std::size_t index = 0; index < colors.size(); ++index)
    {
        REQUIRE(mesh.vertices[index].color == colors[index].getPackedValueProperty());
        REQUIRE(std::abs(mesh.vertices[index].normal.LengthSquared() - 1.0f) < 1.0e-5f);
    }
}

TEST_CASE("textured quads retain explicit repeating coordinates")
{
    MeshBuilder builder;
    const std::array coordinates = {
        Vector2{-1.5f,20.0f},Vector2{-1.5f,26.25f},
        Vector2{3.0f,26.25f},Vector2{3.0f,20.0f}};
    builder.addTexturedQuad(
        Vector3{-1.0f,0.0f,-1.0f},Vector3{-1.0f,0.0f,1.0f},
        Vector3{1.0f,0.0f,1.0f},Vector3{1.0f,0.0f,-1.0f},
        Color(120,160,100,255),coordinates[0],coordinates[1],coordinates[2],coordinates[3]);
    const auto mesh = builder.take();
    REQUIRE(mesh.vertices.size() == coordinates.size());
    for (std::size_t index = 0; index < coordinates.size(); ++index)
    {
        REQUIRE(mesh.vertices[index].textureCoordinate.X == coordinates[index].X);
        REQUIRE(mesh.vertices[index].textureCoordinate.Y == coordinates[index].Y);
    }
}

TEST_CASE("textured quads retain an independent tint at every corner")
{
    MeshBuilder builder;
    const std::array colors = {Color(40,80,120,255),Color(50,90,130,255),
                               Color(20,60,100,255),Color(25,65,105,255)};
    builder.addTexturedQuad(
        Vector3{-1.0f,0.0f,-1.0f},Vector3{-1.0f,0.0f,1.0f},
        Vector3{1.0f,0.0f,1.0f},Vector3{1.0f,0.0f,-1.0f},
        colors[0],colors[1],colors[2],colors[3],
        Vector2{0.0f,0.0f},Vector2{0.0f,2.0f},
        Vector2{2.0f,2.0f},Vector2{2.0f,0.0f});
    const auto mesh = builder.take();
    REQUIRE(mesh.vertices.size() == colors.size());
    for (std::size_t index = 0; index < colors.size(); ++index)
        REQUIRE(mesh.vertices[index].color == colors[index].getPackedValueProperty());
}

TEST_CASE("procedural boxes provide six finite face-normal directions")
{
    MeshBuilder builder;
    builder.addBox(Vector3{-2.0f,-1.0f,-3.0f}, Vector3{2.0f,4.0f,3.0f},
                   Color(210,190,150,255));
    const auto mesh = builder.take();

    REQUIRE(mesh.vertices.size() == 24);
    REQUIRE(mesh.indices.size() == 36);
    bool positiveX = false;
    bool negativeX = false;
    bool positiveY = false;
    bool negativeY = false;
    bool positiveZ = false;
    bool negativeZ = false;
    for (const auto& vertex : mesh.vertices)
    {
        REQUIRE(std::isfinite(vertex.normal.X));
        REQUIRE(std::isfinite(vertex.normal.Y));
        REQUIRE(std::isfinite(vertex.normal.Z));
        REQUIRE(std::abs(vertex.normal.LengthSquared() - 1.0f) < 1.0e-5f);
        positiveX = positiveX || vertex.normal.X > 0.999f;
        negativeX = negativeX || vertex.normal.X < -0.999f;
        positiveY = positiveY || vertex.normal.Y > 0.999f;
        negativeY = negativeY || vertex.normal.Y < -0.999f;
        positiveZ = positiveZ || vertex.normal.Z > 0.999f;
        negativeZ = negativeZ || vertex.normal.Z < -0.999f;
    }
    REQUIRE(positiveX && negativeX && positiveY && negativeY && positiveZ && negativeZ);
}

TEST_CASE("rounded boxes preserve the ordinary box culling convention on every face")
{
    const Vector3 minimum{-2.0f,-1.0f,-3.0f};
    const Vector3 maximum{2.0f,1.0f,3.0f};
    for (int shape = 0; shape < 4; ++shape)
    {
        MeshBuilder builder;
        if (shape == 0) builder.addBox(minimum,maximum,Color::White);
        if (shape == 1) builder.addRoundedBoxXZ(minimum,maximum,0.25f,3,Color::White);
        if (shape == 2) builder.addRoundedBoxXY(minimum,maximum,0.25f,3,Color::White);
        if (shape == 3) builder.addRoundedBoxYZ(minimum,maximum,0.25f,3,Color::White);
        const auto mesh = builder.take();
        for (std::size_t index = 0; index < mesh.indices.size(); index += 3)
        {
            const auto& a = mesh.vertices[mesh.indices[index]].position;
            const auto& b = mesh.vertices[mesh.indices[index+1]].position;
            const auto& c = mesh.vertices[mesh.indices[index+2]].position;
            const Vector3 centre = (a+b+c)/3.0f;
            const Vector3 winding = Vector3::Cross(b-a,c-a);
            // The project's ordinary box uses clockwise exterior faces. Check the actual
            // indexed triangles, not merely the presence of both signs of stored normals.
            REQUIRE(Vector3::Dot(winding,centre) < -1.0e-6f);
            REQUIRE(Vector3::Dot(mesh.vertices[mesh.indices[index]].normal,centre) > 0.0f);
        }
    }
}

TEST_CASE("curved solids have outward lighting normals and clockwise exterior faces")
{
    for (int shape = 0; shape < 3; ++shape)
    {
        MeshBuilder builder;
        if (shape == 0) builder.addCylinder(Vector3{0,-1,0},1,2,12,Color::White);
        if (shape == 1) builder.addCone(Vector3{0,-1,0},1,2,12,Color::White);
        if (shape == 2) builder.addEllipsoid(Vector3::Zero,Vector3{1,2,1},12,6,Color::White);
        const auto mesh = builder.take();
        for (std::size_t i = 0; i < mesh.indices.size(); i += 3)
        {
            const auto& a = mesh.vertices[mesh.indices[i]];
            const auto& b = mesh.vertices[mesh.indices[i+1]];
            const auto& c = mesh.vertices[mesh.indices[i+2]];
            const Vector3 centre = (a.position+b.position+c.position)/3.0f;
            REQUIRE(Vector3::Dot(a.normal,centre) > 0.0f);
            REQUIRE(Vector3::Dot(Vector3::Cross(b.position-a.position,c.position-a.position),centre) < 0.0f);
        }
    }
}

TEST_CASE("rounded profiles include both tangent endpoints of every corner")
{
    for (int shape = 0; shape < 3; ++shape)
    {
        MeshBuilder builder;
        const Vector3 minimum{-2,-2,-2};
        const Vector3 maximum{2,2,2};
        if (shape == 0) builder.addRoundedBoxXZ(minimum,maximum,0.5f,3,Color::White);
        if (shape == 1) builder.addRoundedBoxXY(minimum,maximum,0.5f,3,Color::White);
        if (shape == 2) builder.addRoundedBoxYZ(minimum,maximum,0.5f,3,Color::White);
        const auto mesh = builder.take();
        for (const float signA : {-1.0f,1.0f})
            for (const float signB : {-1.0f,1.0f})
                for (int endpoint = 0; endpoint < 2; ++endpoint)
                {
                    bool found = false;
                    for (const auto& vertex : mesh.vertices)
                    {
                        const auto& p = vertex.position;
                        const float a = shape == 2 ? p.Y : p.X;
                        const float b = shape == 1 ? p.Y : p.Z;
                        found = found || (std::abs(a-signA*(endpoint == 0 ? 2.0f : 1.5f)) < 1e-5f
                                      && std::abs(b-signB*(endpoint == 0 ? 1.5f : 2.0f)) < 1e-5f);
                    }
                    REQUIRE(found);
                }
    }
}

TEST_CASE("rounded XZ boxes retain bounds and opposite top and bottom faces")
{
    MeshBuilder builder;
    const Vector3 minimum{-2.0f,0.4f,-1.0f};
    const Vector3 maximum{2.0f,0.7f,1.0f};
    builder.addRoundedBoxXZ(minimum,maximum,0.2f,3,Color(45,90,120,255));
    const auto mesh = builder.take();

    REQUIRE(mesh.vertices.size() == 160);
    REQUIRE(mesh.indices.size() == 192);
    bool upward = false;
    bool downward = false;
    for (const auto& vertex : mesh.vertices)
    {
        REQUIRE(vertex.position.X >= minimum.X-1.0e-5f);
        REQUIRE(vertex.position.X <= maximum.X+1.0e-5f);
        REQUIRE(vertex.position.Y >= minimum.Y-1.0e-5f);
        REQUIRE(vertex.position.Y <= maximum.Y+1.0e-5f);
        REQUIRE(vertex.position.Z >= minimum.Z-1.0e-5f);
        REQUIRE(vertex.position.Z <= maximum.Z+1.0e-5f);
        REQUIRE(std::abs(vertex.normal.LengthSquared()-1.0f) < 1.0e-5f);
        upward = upward || vertex.normal.Y > 0.999f;
        downward = downward || vertex.normal.Y < -0.999f;
    }
    REQUIRE(upward && downward);
}

TEST_CASE("rounded XY boxes retain bounds and outward front and back faces")
{
    MeshBuilder builder;
    const Vector3 minimum{-0.25f,0.5f,-0.06f};
    const Vector3 maximum{0.25f,1.5f,0.06f};
    builder.addRoundedBoxXY(minimum,maximum,0.06f,3,Color(45,90,120,255));
    const auto mesh = builder.take();

    REQUIRE(mesh.vertices.size() == 160);
    REQUIRE(mesh.indices.size() == 192);
    bool forward = false;
    bool backward = false;
    for (const auto& vertex : mesh.vertices)
    {
        REQUIRE(vertex.position.X >= minimum.X-1.0e-5f);
        REQUIRE(vertex.position.X <= maximum.X+1.0e-5f);
        REQUIRE(vertex.position.Y >= minimum.Y-1.0e-5f);
        REQUIRE(vertex.position.Y <= maximum.Y+1.0e-5f);
        REQUIRE(vertex.position.Z >= minimum.Z-1.0e-5f);
        REQUIRE(vertex.position.Z <= maximum.Z+1.0e-5f);
        REQUIRE(std::abs(vertex.normal.LengthSquared()-1.0f) < 1.0e-5f);
        forward = forward || vertex.normal.Z > 0.999f;
        backward = backward || vertex.normal.Z < -0.999f;
    }
    REQUIRE(forward && backward);
}

TEST_CASE("rounded YZ boxes retain bounds and outward lateral faces")
{
    MeshBuilder builder;
    const Vector3 minimum{-1.35f,0.12f,-0.62f};
    const Vector3 maximum{-0.82f,0.72f,-0.05f};
    builder.addRoundedBoxYZ(minimum,maximum,0.06f,3,Color(120,75,55,255));
    const auto mesh = builder.take();

    REQUIRE(mesh.vertices.size() == 160);
    REQUIRE(mesh.indices.size() == 192);
    bool left = false;
    bool right = false;
    for (const auto& vertex : mesh.vertices)
    {
        REQUIRE(vertex.position.X >= minimum.X-1.0e-5f);
        REQUIRE(vertex.position.X <= maximum.X+1.0e-5f);
        REQUIRE(vertex.position.Y >= minimum.Y-1.0e-5f);
        REQUIRE(vertex.position.Y <= maximum.Y+1.0e-5f);
        REQUIRE(vertex.position.Z >= minimum.Z-1.0e-5f);
        REQUIRE(vertex.position.Z <= maximum.Z+1.0e-5f);
        REQUIRE(std::abs(vertex.normal.LengthSquared()-1.0f) < 1.0e-5f);
        left = left || vertex.normal.X < -0.999f;
        right = right || vertex.normal.X > 0.999f;
    }
    REQUIRE(left && right);
}

TEST_CASE("horizontal discs face upward for train-floor contact shadows")
{
    MeshBuilder builder;
    builder.addDiscXZ(Vector3::Zero, 1.0f, 16, Color(8,12,14,82));
    const auto mesh = builder.take();

    REQUIRE(mesh.vertices.size() == 48);
    REQUIRE(mesh.indices.size() == 48);
    for (const auto& vertex : mesh.vertices)
    {
        REQUIRE(std::abs(vertex.normal.LengthSquared() - 1.0f) < 1.0e-5f);
        REQUIRE(vertex.normal.Y > 0.999f);
    }
}

TEST_CASE("faceted ellipsoids stay bounded and carry finite unit normals")
{
    MeshBuilder builder;
    const Vector3 centre{3.0f,4.0f,-2.0f};
    const Vector3 radii{2.0f,1.5f,0.75f};
    builder.addEllipsoid(centre,radii,8,4,Color(50,120,60,255));
    const auto mesh = builder.take();

    REQUIRE(!mesh.empty());
    REQUIRE(mesh.indices.size() % 3 == 0);
    for (const auto& vertex : mesh.vertices)
    {
        REQUIRE(std::isfinite(vertex.normal.X));
        REQUIRE(std::isfinite(vertex.normal.Y));
        REQUIRE(std::isfinite(vertex.normal.Z));
        REQUIRE(std::abs(vertex.normal.LengthSquared() - 1.0f) < 1.0e-5f);
        REQUIRE(vertex.position.X >= centre.X-radii.X-1.0e-5f);
        REQUIRE(vertex.position.X <= centre.X+radii.X+1.0e-5f);
        REQUIRE(vertex.position.Y >= centre.Y-radii.Y-1.0e-5f);
        REQUIRE(vertex.position.Y <= centre.Y+radii.Y+1.0e-5f);
        REQUIRE(vertex.position.Z >= centre.Z-radii.Z-1.0e-5f);
        REQUIRE(vertex.position.Z <= centre.Z+radii.Z+1.0e-5f);
    }
}

TEST_CASE("smooth ellipsoid normals follow the surface gradient without recolouring facets")
{
    MeshBuilder smooth;
    MeshBuilder faceted;
    const Vector3 centre{1,2,3};
    const Vector3 radii{2,1,0.5f};
    const Color colour(180,190,200,255);
    smooth.addEllipsoid(centre,radii,12,6,colour,true);
    faceted.addEllipsoid(centre,radii,12,6,colour);
    const auto mesh = smooth.take();
    const auto original = faceted.take();
    REQUIRE(mesh.indices == original.indices);
    REQUIRE(mesh.vertices.size() == original.vertices.size());
    for (const auto& vertex : mesh.vertices)
    {
        const Vector3 offset = vertex.position-centre;
        Vector3 expected{offset.X/4.0f,offset.Y,offset.Z/0.25f};
        expected.Normalize();
        REQUIRE((vertex.normal-expected).LengthSquared() < 1e-8f);
        REQUIRE(std::abs(vertex.normal.LengthSquared()-1.0f) < 1e-5f);
        REQUIRE(vertex.color == colour.getPackedValueProperty());
    }
}

TEST_CASE("viaduct valley falls from seamless abutments to a deep crossing floor")
{
    using namespace TrainSim::Render;
    const double begin = TrainSim::Sim::kValleyViaductBeginMetres;
    const double end = TrainSim::Sim::kValleyViaductEndMetres;
    const double middle = (begin+end)*0.5;
    for (double lateral : {-1'200.0,-260.0,-12.0,-2.5,0.0,3.0,40.0,600.0,1'200.0})
    {
        // Outside and exactly at both abutments the natural approach terrain is unchanged.
        for (double distance : {begin-500.0,begin,end,end+500.0})
            REQUIRE(viaductValleyTerrain(distance,lateral,4.25) == 4.25);
        // Close to the abutment the change is small and continuous.
        REQUIRE(std::abs(viaductValleyTerrain(begin+1.0,lateral,-0.34)+0.34) < 0.3);
        REQUIRE(std::abs(viaductValleyTerrain(end-1.0,lateral,-0.34)+0.34) < 0.3);
        // The valley runs across the railway: its floor is deep at every lateral offset.
        REQUIRE(viaductValleyTerrain(middle,lateral,30.0) < -28.0);
        REQUIRE(viaductValleyTerrain(middle,lateral,30.0) > -44.0);
        // A broad bowl rather than a flat plain: the floor keeps falling toward mid-span.
        REQUIRE(viaductValleyFloor(begin+700.0,lateral) > viaductValleyFloor(middle,lateral)+3.0);
        double previous = -1.0;
        for (double inside = 0.0; inside <= 700.0; inside += 10.0)
        {
            const double weight = viaductValleyWeight(begin+inside,lateral);
            REQUIRE(weight >= previous);
            REQUIRE(weight >= 0.0);
            REQUIRE(weight <= 1.0);
            previous = weight;
            REQUIRE(std::abs(weight-viaductValleyWeight(end-inside,lateral)) < 0.35);
        }
        REQUIRE(previous == 1.0);
    }
}

TEST_CASE("bridge handrail tubes are slender with outward faces and closed ends")
{
    using namespace TrainSim::Render;
    MeshBuilder builder;
    addBridgeRail(builder,0.5f,1.2f,10.0f,0.0f,kBridgeHandrailRadius,20.0f,8,true,true,
                  Color(150,155,152,255));
    const auto mesh = builder.take();
    REQUIRE(mesh.indices.size() == 8U*(6U+3U+3U));
    bool beginCap = false;
    bool endCap = false;
    for (const auto& vertex : mesh.vertices)
    {
        const Vector3 radial{vertex.position.X-0.5f,vertex.position.Y-1.2f,0.0f};
        REQUIRE(radial.Length() <= kBridgeHandrailRadius+1.0e-5f);
        REQUIRE(std::abs(vertex.normal.LengthSquared()-1.0f) < 1.0e-4f);
        if (std::abs(vertex.normal.Z) > 0.999f)
        {
            beginCap = beginCap || (vertex.normal.Z < 0.0f && vertex.position.Z < 0.01f);
            endCap = endCap || (vertex.normal.Z > 0.0f && vertex.position.Z > 19.99f);
        }
        else if (radial.Length() > 0.01f)
            REQUIRE(vertex.normal.X*radial.X+vertex.normal.Y*radial.Y > 0.0f);
    }
    REQUIRE(beginCap);
    REQUIRE(endCap);
}

TEST_CASE("bridge parapet replaces broad beams with a bounded post and rail assembly")
{
    using namespace TrainSim::Render;
    struct Point { float x,y,z,yaw; };
    const auto point = [](double distance,double lateral) {
        return Point{static_cast<float>(lateral),0.0f,static_cast<float>(distance),0.0f};
    };
    for (const float outward : {-1.0f,1.0f})
    {
        const double lateral = outward > 0.0f ? 3.0 : -8.0;
        MeshBuilder builder;
        const int posts = addBridgeParapet(builder,point,40.0,60.0,lateral,outward,false,false,
                                           bridgeParapetStyle(false));
        REQUIRE(posts == 8);
        MeshBuilder lowBuilder;
        REQUIRE(addBridgeParapet(lowBuilder,point,40.0,60.0,lateral,outward,false,false,
                                 bridgeParapetStyle(false),false) == 8);
        const auto mesh = builder.take();
        REQUIRE(!mesh.empty());
        REQUIRE(mesh.indices.size()/3U < 400U);
        REQUIRE(lowBuilder.take().indices.size() < mesh.indices.size());
        const float railCentre = static_cast<float>(lateral)-0.02f*outward;
        float highest = -1.0f;
        for (const auto& vertex : mesh.vertices)
        {
            const float across = (vertex.position.X-static_cast<float>(lateral))*outward;
            REQUIRE(across >= -0.84f);
            REQUIRE(across <= 0.27f);
            REQUIRE(vertex.position.Y >= -0.64f);
            REQUIRE(vertex.position.Z >= 39.93f);
            REQUIRE(vertex.position.Z <= 60.07f);
            highest = std::max(highest,vertex.position.Y);
            // Everything above the coping is slender steel: posts and two round rails.
            if (vertex.position.Y > kBridgeParapetTop+0.03f)
                REQUIRE(std::abs(vertex.position.X-railCentre)
                        <= std::max(kBridgePostWidth*0.5f,kBridgeHandrailRadius)+1.0e-4f);
            if (vertex.position.Y > kBridgeHandrailHeight-kBridgeHandrailRadius-1.0e-4f)
                REQUIRE(std::abs(vertex.position.X-railCentre) <= kBridgeHandrailRadius+1.0e-4f);
        }
        REQUIRE(std::abs(highest-(kBridgeHandrailHeight+kBridgeHandrailRadius)) < 1.0e-3f);
    }
}

TEST_CASE("tunnel escape and chainage labels name the nearer portal and hectometre")
{
    using namespace TrainSim::Render;
    REQUIRE(!tunnelExtentAt(120'999.0).has_value());
    REQUIRE(!tunnelExtentAt(177'900.0).has_value());
    const auto longTunnel = tunnelExtentAt(123'000.0);
    REQUIRE(longTunnel.has_value());
    REQUIRE(longTunnel->first == 121'000.0);
    REQUIRE(longTunnel->second == 126'800.0);
    REQUIRE(tunnelExitLabel(123'000.0,121'000.0,126'800.0) == "< 2.0 KM");
    REQUIRE(tunnelExitLabel(126'000.0,121'000.0,126'800.0) == "800 M >");
    REQUIRE(tunnelExitLabel(121'340.0,121'000.0,126'800.0) == "< 340 M");
    REQUIRE(tunnelExitLabel(230'300.0,229'500.0,231'100.0) == "< 800 M");
    REQUIRE(tunnelExitLabel(230'400.0,229'500.0,231'100.0) == "700 M >");
    REQUIRE(tunnelChainageLabel(123'000.0) == "123.0");
    REQUIRE(tunnelChainageLabel(123'499.0) == "123.4");
    REQUIRE(tunnelChainageLabel(229'500.0) == "229.5");
}

namespace {
struct TunnelTestPoint { float x,y,z,yaw; };
TunnelTestPoint tunnelTestPoint(double distance,double lateral)
{
    return {static_cast<float>(lateral),0.0f,static_cast<float>(distance),0.0f};
}
}

TEST_CASE("tunnel lining segments stay on their wall face and look toward the track")
{
    using namespace TrainSim::Render;
    for (const auto& [face,inward] : {std::pair{3.925,-1.0f},std::pair{-8.925,1.0f}})
    {
        MeshBuilder structure;
        MeshBuilder lining;
        const int rings = addTunnelLiningSegment(structure,lining,tunnelTestPoint,40.0,60.0,
                                                 face,inward,true,tunnelLiningStyle());
        REQUIRE(rings == 10);
        MeshBuilder lowStructure;
        MeshBuilder lowLining;
        REQUIRE(addTunnelLiningSegment(lowStructure,lowLining,tunnelTestPoint,40.0,60.0,
                                       face,inward,false,tunnelLiningStyle()) == 10);
        REQUIRE(lowStructure.data().indices.size() < structure.data().indices.size());
        for (const MeshBuilder* builder : {&structure,&lining})
        {
            const auto& mesh = builder->data();
            REQUIRE(!mesh.empty());
            for (const auto& vertex : mesh.vertices)
            {
                const float inboard = (vertex.position.X-static_cast<float>(face))*inward;
                REQUIRE(inboard >= 0.0f);
                REQUIRE(inboard <= 0.56f);
                REQUIRE(vertex.position.Y >= kTunnelLiningBottom-1.0e-4f);
                REQUIRE(vertex.position.Y <= 6.78f+1.0e-4f);
                REQUIRE(vertex.position.Z >= 39.9f);
                REQUIRE(vertex.position.Z <= 60.1f);
                REQUIRE(vertex.normal.X*inward > 0.2f);
            }
        }
        // The textured face tiles the shared concrete material at a readable panel scale.
        bool tiled = false;
        for (const auto& vertex : lining.data().vertices)
            tiled = tiled || vertex.textureCoordinate.X > 30.0f;
        REQUIRE(tiled);
    }
}

TEST_CASE("tunnel wall luminaire has a thin lens and a seamless centred wash")
{
    using namespace TrainSim::Render;
    const double face = 3.925;
    const float inward = -1.0f;
    MeshBuilder structure;
    MeshBuilder lining;
    MeshBuilder emitter;
    addTunnelWallLuminaire(structure,lining,emitter,tunnelTestPoint,50.0,face,inward,
                           tunnelLiningStyle());
    REQUIRE(!structure.data().empty());
    for (const auto& vertex : emitter.data().vertices)
    {
        const float inboard = (vertex.position.X-static_cast<float>(face))*inward;
        REQUIRE(inboard >= 0.01f);
        REQUIRE(inboard <= 0.28f);
        REQUIRE(std::abs(vertex.position.Z-50.0f) <= 0.50f);
    }
    const auto wash = lining.data();
    REQUIRE(wash.vertices.size() == 16);
    const std::uint32_t liningColour = tunnelLiningStyle().lining.getPackedValueProperty();
    int centreVertices = 0;
    for (const auto& vertex : wash.vertices)
    {
        const bool centre = std::abs(vertex.position.Z-50.0f) < 1.0e-4f
                         && std::abs(vertex.position.Y-kTunnelLuminaireHeight) < 1.0e-4f;
        if (centre) ++centreVertices;
        else REQUIRE(vertex.color == liningColour);
        REQUIRE(std::abs(vertex.position.Z-50.0f) <= 1.9f+1.0e-4f);
        REQUIRE(vertex.normal.X*inward > 0.99f);
    }
    REQUIRE(centreVertices == 4);
    for (const auto& vertex : wash.vertices)
        if (std::abs(vertex.position.Z-50.0f) < 1.0e-4f
            && std::abs(vertex.position.Y-kTunnelLuminaireHeight) < 1.0e-4f)
            REQUIRE(vertex.color != liningColour);
}

TEST_CASE("ballast shoulder toe wobbles within bounds and meets between segments")
{
    using TrainSim::Render::ballastShoulderToe;
    float previous = ballastShoulderToe(0.0);
    for (double distance = 0.0; distance <= 3'000.0; distance += 0.5)
    {
        const float toe = ballastShoulderToe(distance);
        REQUIRE(toe >= 3.10f);
        REQUIRE(toe <= 3.62f);
        REQUIRE(std::abs(toe-previous) < 0.12f);
        previous = toe;
    }
    REQUIRE(ballastShoulderToe(52'000.0) == ballastShoulderToe(52'000.0));
}

TEST_CASE("concrete sleeper profile is chamfered within the box envelope and outward facing")
{
    using namespace TrainSim::Render;
    using Microsoft::Xna::Framework::Matrix;
    const Matrix placement = Matrix::CreateTranslation(0.0f,0.0f,7.0f);
    MeshBuilder simple;
    addConcreteSleeper(simple,placement,-5.0f,Color(127,124,116,255),false);
    MeshBuilder detailed;
    addConcreteSleeper(detailed,placement,-5.0f,Color(127,124,116,255),true);
    REQUIRE(simple.data().indices.size() == 36);
    REQUIRE(detailed.data().indices.size() == 90);
    int topVertices = 0;
    for (const auto& vertex : detailed.data().vertices)
    {
        REQUIRE(vertex.position.X >= -5.0f-kSleeperHalfLength-1.0e-5f);
        REQUIRE(vertex.position.X <= -5.0f+kSleeperHalfLength+1.0e-5f);
        REQUIRE(vertex.position.Y >= -0.10f-1.0e-5f);
        REQUIRE(vertex.position.Y <= 1.0e-5f);
        REQUIRE(std::abs(vertex.position.Z-7.0f) <= kSleeperHalfWidth+1.0e-5f);
        REQUIRE(std::abs(vertex.normal.LengthSquared()-1.0f) < 1.0e-4f);
        if (vertex.position.Y > -1.0e-5f)
        {
            ++topVertices;
            // Rail-seat tops are narrower than the base: the long sides are chamfers.
            REQUIRE(std::abs(vertex.position.Z-7.0f) <= kSleeperTopHalfWidth+1.0e-5f);
        }
        // Side normals lean outward (away from the sleeper axis), never into the block.
        if (std::abs(vertex.normal.Z) > 0.5f)
            REQUIRE(vertex.normal.Z*(vertex.position.Z-7.0f) > 0.0f);
    }
    REQUIRE(topVertices == 24);
}

TEST_CASE("rail profile has a broad foot slender web and wider head below the running surface")
{
    using namespace TrainSim::Render;
    MeshBuilder track;
    addRailProfile(track,Microsoft::Xna::Framework::Matrix::getIdentityProperty(),0.76f,3.0f);
    REQUIRE(track.data().indices.size() == 3U*36U);
    float highest = -1.0f;
    for (const auto& vertex : track.data().vertices)
    {
        const float across = std::abs(vertex.position.X-0.76f);
        REQUIRE(across <= kRailFootHalfWidth+1.0e-5f);
        if (vertex.position.Y > kRailFootTop+1.0e-4f && vertex.position.Y < kRailHeadBottom-1.0e-4f)
            REQUIRE(across <= kRailWebHalfWidth+1.0e-5f);
        if (vertex.position.Y > kRailHeadBottom+1.0e-4f)
            REQUIRE(across <= kRailHeadHalfWidth+1.0e-5f);
        highest = std::max(highest,vertex.position.Y);
    }
    REQUIRE(std::abs(highest-kRailHeadTop) < 1.0e-5f);
    REQUIRE(kRailHeadHalfWidth > kRailWebHalfWidth*3.0f);
    REQUIRE(kRailFootHalfWidth > kRailHeadHalfWidth);
}

TEST_CASE("bars between points follow their chord including pitch")
{
    using namespace TrainSim::Render;
    MeshBuilder mesh;
    const Vector3 a{1.0f,5.0f,-10.0f},b{3.0f,6.0f,10.0f};
    addBarBetween(mesh,a,b,0.04f,Color(72,76,73,255));
    REQUIRE(mesh.data().indices.size() == 36);
    const Vector3 axis = Vector3::Normalize(b-a);
    float nearA = 1.0e9f,nearB = 1.0e9f;
    for (const auto& vertex : mesh.data().vertices)
    {
        // Every corner lies within half the thickness of the chord.
        const Vector3 relative = vertex.position-a;
        const float along = Vector3::Dot(relative,axis);
        const Vector3 perpendicular = relative-axis*along;
        REQUIRE(perpendicular.Length() <= 0.04f*0.7072f+1.0e-4f);
        nearA = std::min(nearA,along);
        nearB = std::min(nearB,(b-a).Length()-along);
    }
    REQUIRE(std::abs(nearA) < 1.0e-3f);
    REQUIRE(std::abs(nearB) < 1.0e-3f);
}

TEST_CASE("lineside fence keeps absolute post cadence strands and strained ends")
{
    using namespace TrainSim::Render;
    const auto point = [](double distance,double lateral) {
        return tunnelTestPoint(distance,lateral);
    };
    MeshBuilder open;
    REQUIRE(addLinesideFence(open,point,250.0,500.0,kFenceLateral,false,false) == 83);
    MeshBuilder closed;
    REQUIRE(addLinesideFence(closed,point,250.0,500.0,-kFenceLateral,true,true) == 85);
    REQUIRE(closed.data().indices.size() == open.data().indices.size()+4U*36U);
    float highest = 0.0f;
    for (const auto& vertex : open.data().vertices)
    {
        REQUIRE(vertex.position.Z >= 250.0f-0.05f);
        REQUIRE(vertex.position.Z <= 500.0f+0.05f);
        REQUIRE(std::abs(vertex.position.X-kFenceLateral) <= 0.04f);
        REQUIRE(vertex.position.Y >= -1.0e-4f);
        highest = std::max(highest,vertex.position.Y);
    }
    REQUIRE(std::abs(highest-kFencePostHeight) < 1.0e-4f);
    // Struts lean inward from each end post and never project past the fence run.
    for (const auto& vertex : closed.data().vertices)
    {
        REQUIRE(vertex.position.Z >= 250.0f-0.06f);
        REQUIRE(vertex.position.Z <= 500.0f+0.06f);
    }
    TrainSim::Sim::RoutePose pose;
    REQUIRE(linesideFenceAllowed(pose));
    pose.urbanity = 0.7;
    REQUIRE(!linesideFenceAllowed(pose));
    pose.urbanity = 0.1;
    pose.bridge = true;
    REQUIRE(!linesideFenceAllowed(pose));
}

TEST_CASE("colour light signal keeps lens centres where the live aspect pass draws")
{
    using namespace TrainSim::Render;
    for (const int facing : {1,-1})
        for (const double lateral : {-6.1,6.1})
        {
            MeshBuilder mesh;
            addColourLightSignal(mesh,tunnelTestPoint,1'000.0,lateral,facing,true);
            MeshBuilder low;
            addColourLightSignal(low,tunnelTestPoint,1'000.0,lateral,facing,false);
            REQUIRE(low.data().indices.size() < mesh.data().indices.size());
            const float side = lateral < 0.0 ? -1.0f : 1.0f;
            const float lensX = static_cast<float>(lateral)-side*static_cast<float>(kSignalLensInset);
            const float faceZ = 1'000.0f-static_cast<float>(kSignalLensDepth*facing);
            float highest = 0.0f;
            std::array<int,3> lensFaces{};
            for (const auto& vertex : mesh.data().vertices)
            {
                REQUIRE(std::abs(vertex.position.Z-1'000.0f) <= 1.8f);
                REQUIRE(std::abs(vertex.position.X-static_cast<float>(lateral)) <= 1.0f);
                REQUIRE(vertex.position.Y >= -1.0e-4f);
                highest = std::max(highest,vertex.position.Y);
                // The recessed dark lens boxes are 0.23 m squares centred on the lens axis.
                if (vertex.color == Color(16,19,18,255).getPackedValueProperty())
                    for (std::size_t index = 0; index < kSignalLensHeights.size(); ++index)
                        if (std::abs(vertex.position.Y-kSignalLensHeights[index]) < 0.116f
                            && std::abs(vertex.position.X-lensX) < 0.116f
                            && std::abs(vertex.position.Z-faceZ) < 0.04f)
                            ++lensFaces[index];
            }
            // addBox shades five faces; only the unshaded face keeps the exact lens colour.
            for (int count : lensFaces) REQUIRE(count == 4);
            REQUIRE(highest > 4.75f);
            REQUIRE(highest < 5.0f);
        }
}

TEST_CASE("seat pedestal stays inside the old footprint with a stepped base")
{
    using namespace TrainSim::Render;
    MeshBuilder mesh;
    addSeatPedestal(mesh,0.55f,-12.0f,Color(95,99,99,255));
    float lowest = 1.0f,highest = 0.0f,widest = 0.0f;
    for (const auto& vertex : mesh.data().vertices)
    {
        const float dx = vertex.position.X-0.55f,dz = vertex.position.Z+12.0f;
        widest = std::max(widest,std::sqrt(dx*dx+dz*dz));
        lowest = std::min(lowest,vertex.position.Y);
        highest = std::max(highest,vertex.position.Y);
        // Nothing above the base plate is as wide as the plate: the profile steps inward.
        if (vertex.position.Y > 0.12f) REQUIRE(std::sqrt(dx*dx+dz*dz) <= 0.065f);
    }
    REQUIRE(widest <= 0.1251f);
    REQUIRE(std::abs(lowest-0.08f) < 1.0e-5f);
    REQUIRE(highest <= 0.42f);
}

TEST_CASE("rack suitcase handle and castors stay within the shelf envelope")
{
    using namespace TrainSim::Render;
    for (const bool wheeled : {false,true})
    {
        MeshBuilder mesh;
        addRackSuitcase(mesh,0.93f,1.19f,20.0f,0.43f,2.46f,Color(125,67,52,255),wheeled);
        float highest = 0.0f;
        for (const auto& vertex : mesh.data().vertices)
        {
            REQUIRE(vertex.position.X >= 0.93f-0.005f);
            REQUIRE(vertex.position.X <= 1.19f+0.005f);
            REQUIRE(vertex.position.Y >= 2.23f-0.02f);
            REQUIRE(std::abs(vertex.position.Z-20.0f) <= 0.43f+0.005f);
            highest = std::max(highest,vertex.position.Y);
        }
        REQUIRE(std::abs(highest-(2.46f+0.075f)) < 1.0e-4f);
    }
    MeshBuilder plain,wheeledCase;
    addRackSuitcase(plain,0.93f,1.19f,20.0f,0.43f,2.46f,Color(125,67,52,255),false);
    addRackSuitcase(wheeledCase,0.93f,1.19f,20.0f,0.43f,2.46f,Color(125,67,52,255),true);
    REQUIRE(wheeledCase.data().indices.size() > plain.data().indices.size());
}

TEST_CASE("cab door hardware projects from the saloon face only and clears the warning band")
{
    using namespace TrainSim::Render;
    for (const auto& [z,face] : {std::pair{-63.2f,1.0f},std::pair{63.2f,-1.0f}})
    {
        MeshBuilder mesh;
        addCabDoorHardware(mesh,z,face);
        const float surface = z+face*0.10f;
        const std::uint32_t handle = Color(222,186,53,255).getPackedValueProperty();
        bool sawHandle = false;
        for (const auto& vertex : mesh.data().vertices)
        {
            REQUIRE((vertex.position.Z-surface)*face >= -1.0e-4f);
            REQUIRE((vertex.position.Z-surface)*face <= 0.076f);
            REQUIRE(std::abs(vertex.position.X) <= 0.60f+1.0e-5f);
            if (vertex.color == handle)
            {
                sawHandle = true;
                REQUIRE(vertex.position.Y < 0.72f);
                REQUIRE(vertex.position.X > 0.2f);
            }
        }
        REQUIRE(sawHandle);
    }
}

TEST_CASE("gangway bridge plate grooves stay flush and the emergency intercom is bounded")
{
    using namespace TrainSim::Render;
    MeshBuilder plate;
    addGangwayBridgePlate(plate,31.5f);
    for (const auto& vertex : plate.data().vertices)
    {
        REQUIRE(vertex.position.Y >= 0.08f-1.0e-5f);
        REQUIRE(vertex.position.Y <= 0.104f+1.0e-5f);
        REQUIRE(std::abs(vertex.position.Z-31.5f) <= 0.34f+1.0e-5f);
    }
    MeshBuilder intercom;
    addEmergencyIntercom(intercom,-1.0f,63.08f);
    for (const auto& vertex : intercom.data().vertices)
    {
        REQUIRE(std::abs(vertex.position.X+1.0f) <= 0.23f+1.0e-5f);
        REQUIRE(vertex.position.Y >= 1.05f-1.0e-5f);
        REQUIRE(vertex.position.Y <= 1.78f+1.0e-5f);
        REQUIRE(vertex.position.Z <= 63.08f-0.020f+1.0e-5f);
        REQUIRE(vertex.position.Z >= 63.08f-0.075f-0.045f);
    }
}

TEST_CASE("toilet compartment fittings stay inside the compartment and off the floor")
{
    using namespace TrainSim::Render;
    MeshBuilder mesh;
    addTrainToilet(mesh);
    addToiletSupportRail(mesh);
    addPaperDispenser(mesh);
    addToiletFloorCove(mesh);
    addSinkVanity(mesh);
    addToiletMirror(mesh);
    addWashroomDispensers(mesh);
    REQUIRE(mesh.data().indices.size()/3U < 4'000U);
    for (const auto& vertex : mesh.data().vertices)
    {
        REQUIRE(vertex.position.X >= 0.40f);
        REQUIRE(vertex.position.X <= 1.351f);
        REQUIRE(vertex.position.Y >= kToiletFloorY-1.0e-4f);
        REQUIRE(vertex.position.Y <= 2.20f);
        REQUIRE(vertex.position.Z >= 3.95f);
        REQUIRE(vertex.position.Z <= 8.0f);
    }
    MeshBuilder threshold;
    addToiletThreshold(threshold);
    for (const auto& vertex : threshold.data().vertices)
    {
        REQUIRE(vertex.position.X >= 0.30f-1.0e-5f);
        REQUIRE(vertex.position.X <= 0.50f+1.0e-5f);
        REQUIRE(vertex.position.Y <= kToiletFloorY+0.0141f);
        REQUIRE(vertex.position.Z >= 3.55f);
        REQUIRE(vertex.position.Z <= 4.59f);
    }
}

TEST_CASE("toilet mirror gradient is paler toward the ceiling")
{
    using namespace TrainSim::Render;
    MeshBuilder mesh;
    addToiletMirror(mesh);
    int topLuma = 0,bottomLuma = 0;
    for (const auto& vertex : mesh.data().vertices)
    {
        if (std::abs(vertex.position.X-1.322f) > 0.002f) continue;
        const int r = static_cast<int>(vertex.color&0xffU);
        const int g = static_cast<int>((vertex.color>>8U)&0xffU);
        const int b = static_cast<int>((vertex.color>>16U)&0xffU);
        const int luma = r+g+b;
        if (vertex.position.Y > 2.0f) topLuma = std::max(topLuma,luma);
        if (vertex.position.Y < 1.1f && vertex.position.Z < 4.5f) bottomLuma = std::max(bottomLuma,luma);
    }
    REQUIRE(topLuma > bottomLuma+100);
}

TEST_CASE("ceiling diffuser keeps its footprint with slats recessed behind a proud frame")
{
    using namespace TrainSim::Render;
    MeshBuilder mesh;
    addCeilingDiffuser(mesh,0.56f,-34.2f);
    const std::uint32_t slat = Color(160,168,167,255).getPackedValueProperty();
    const std::uint32_t trim = Color(188,195,194,255).getPackedValueProperty();
    float lowestTrim = 3.0f,lowestSlat = 3.0f,highest = 0.0f;
    int slatVertices = 0;
    for (const auto& vertex : mesh.data().vertices)
    {
        REQUIRE(std::abs(vertex.position.X-0.56f) <= 0.20f+1.0e-5f);
        REQUIRE(std::abs(vertex.position.Z+34.2f) <= 0.34f+1.0e-5f);
        REQUIRE(vertex.position.Y >= 2.796f-1.0e-5f);
        REQUIRE(vertex.position.Y <= 2.834f+1.0e-5f);
        highest = std::max(highest,vertex.position.Y);
        if (vertex.color == trim) lowestTrim = std::min(lowestTrim,vertex.position.Y);
        if (vertex.color == slat)
        {
            ++slatVertices;
            lowestSlat = std::min(lowestSlat,vertex.position.Y);
        }
    }
    REQUIRE(slatVertices == 7*2*4);
    REQUIRE(lowestTrim < lowestSlat);
    // Slats must face the saloon below: every slat normal points downward.
    for (const auto& vertex : mesh.data().vertices)
        if (vertex.color == slat) REQUIRE(vertex.normal.Y < -0.5f);
    REQUIRE(highest > 2.829f);
}

TEST_CASE("lower bodyside keeps bogies and equipment within the body width above the rail")
{
    using namespace TrainSim::Render;
    for (const float side : {-1.0f,1.0f})
    {
        MeshBuilder mesh;
        addLowerBodyside(mesh,side,-62.75f,-42.25f,-52.5f,-47.0f,-45.4f,Color(211,218,218,255));
        addLiveryBands(mesh,side,-62.75f,-42.25f,-47.0f,-45.4f,Color(32,102,121,255));
        float lowest = 1.0f;
        int wheelRims = 0;
        for (const auto& vertex : mesh.data().vertices)
        {
            REQUIRE(vertex.position.X*side >= 0.99f);
            REQUIRE(vertex.position.X*side <= 1.509f);
            REQUIRE(vertex.position.Y >= -1.0e-5f);
            REQUIRE(vertex.position.Y <= 0.86f+1.0e-5f);
            REQUIRE(vertex.position.Z >= -62.76f);
            REQUIRE(vertex.position.Z <= -42.24f);
            lowest = std::min(lowest,vertex.position.Y);
            // Only the wheels reach the rail plane; every other part floats above it.
            if (vertex.position.Y < 1.0e-4f) ++wheelRims;
        }
        REQUIRE(std::abs(lowest) < 1.0e-5f);
        REQUIRE(wheelRims > 0);
    }
}

TEST_CASE("roof equipment and pantograph stay below the contact wire and above the roof")
{
    using namespace TrainSim::Render;
    MeshBuilder plain,powered;
    addRoofEquipment(plain,-62.75f,-42.25f,-52.5f,false);
    addRoofEquipment(powered,-41.75f,-21.25f,-31.5f,true);
    REQUIRE(powered.data().indices.size() > plain.data().indices.size());
    float highest = 0.0f;
    for (const auto& vertex : powered.data().vertices)
    {
        REQUIRE(vertex.position.Y >= kRoofTop-1.0e-5f);
        REQUIRE(std::abs(vertex.position.X) <= 1.48f+1.0e-5f);
        highest = std::max(highest,vertex.position.Y);
    }
    REQUIRE(highest > 5.80f);
    REQUIRE(highest < 5.85f);
    for (const auto& vertex : plain.data().vertices) REQUIRE(vertex.position.Y < 3.60f);
}

TEST_CASE("cab exterior projects forward of the nose with lamps in the emitter batch")
{
    using namespace TrainSim::Render;
    for (const float direction : {-1.0f,1.0f})
    {
        const float nose = direction*72.8f;
        MeshBuilder mesh,emitters;
        addCabExterior(mesh,emitters,direction,nose,Color(32,102,121,255));
        REQUIRE(!emitters.data().empty());
        for (const auto& vertex : mesh.data().vertices)
        {
            REQUIRE((vertex.position.Z-nose)*direction >= -0.60f);
            REQUIRE((vertex.position.Z-nose)*direction <= 1.08f);
            REQUIRE(std::abs(vertex.position.X) <= 1.52f);
            REQUIRE(vertex.position.Y >= -1.0e-5f);
            REQUIRE(vertex.position.Y <= 3.25f);
        }
        for (const auto& vertex : emitters.data().vertices)
            REQUIRE((vertex.position.Z-nose)*direction > 0.07f);
    }
}

TEST_CASE("terminus kit closes the tracks beyond the platform with stops shed and head house")
{
    using namespace TrainSim::Render;
    for (const double inward : {1.0,-1.0})
    {
        const double datum = inward > 0 ? 0.0 : 286'400.0;
        MeshBuilder stops;
        addBufferStop(stops,tunnelTestPoint,datum-inward*kTerminalBufferOffset,-2.5,inward);
        float stopHighest = 0.0f;
        for (const auto& vertex : stops.data().vertices)
        {
            REQUIRE(std::abs(vertex.position.X+2.5f) <= 1.1f);
            REQUIRE(vertex.position.Y >= 0.1f);
            REQUIRE((vertex.position.Z-static_cast<float>(datum))*inward <= -(kTerminalBufferOffset-3.0));
            // Struts lean away from the train, 2.6 m beyond the beam.
            REQUIRE((vertex.position.Z-static_cast<float>(datum))*inward >= -(kTerminalBufferOffset+3.2));
            stopHighest = std::max(stopHighest,vertex.position.Y);
        }
        REQUIRE(stopHighest > 1.4f);
        MeshBuilder house;
        addTerminusHeadHouse(house,tunnelTestPoint,datum,inward,-2.5);
        float left = 0.0f,right = 0.0f,top = 0.0f;
        for (const auto& vertex : house.data().vertices)
        {
            left = std::min(left,vertex.position.X);
            right = std::max(right,vertex.position.X);
            top = std::max(top,vertex.position.Y);
            const float along = (vertex.position.Z-static_cast<float>(datum))*static_cast<float>(inward);
            REQUIRE(along <= -(kTerminalHeadHouseNear-5.0));
            REQUIRE(along >= -(kTerminalHeadHouseFar+2.0));
        }
        REQUIRE(left < -30.0f);
        REQUIRE(right > 25.0f);
        REQUIRE(top > 15.0f);
        MeshBuilder shed;
        REQUIRE(addTrainShed(shed,tunnelTestPoint,datum,inward,-2.5) == 18);
        for (const auto& vertex : shed.data().vertices)
        {
            REQUIRE(std::abs(vertex.position.X+2.5f) <= 16.2f);
            const float along = (vertex.position.Z-static_cast<float>(datum))*static_cast<float>(inward);
            REQUIRE(along <= -(kTerminalShedBegin-1.0));
            REQUIRE(along >= -(kTerminalShedEnd+1.0));
            // Only column feet and shafts reach below the canopy level; the roof clears it.
            if (std::abs(std::abs(vertex.position.X+2.5f)-15.5f) > 0.6f)
                REQUIRE(vertex.position.Y >= 7.1f);
        }
    }
    MeshBuilder rank;
    REQUIRE(addTaxiRank(rank,[](float along,float across) {
        return Vector3{across,0.78f,along};
    },0.0f,0U) == 4);
    for (const auto& vertex : rank.data().vertices)
    {
        REQUIRE(vertex.position.Y >= 0.78f);
        REQUIRE(vertex.position.X >= 19.0f);
        REQUIRE(vertex.position.X <= 26.0f);
    }
}

TEST_CASE("world prefetch selection respects both terminal directions")
{
    using TrainSim::Render::nextWorldChunk;
    REQUIRE(nextWorldChunk(208,1,286400,250)==209);
    REQUIRE(nextWorldChunk(208,-1,286400,250)==207);
    REQUIRE(nextWorldChunk(0,-1,286400,250)==0);
    REQUIRE(nextWorldChunk(0,1,286400,250)==1);
    REQUIRE(nextWorldChunk(1145,1,286400,250)==1145);
    REQUIRE(nextWorldChunk(1145,-1,286400,250)==1144);
    REQUIRE(nextWorldChunk(0,1,100,250)==0);
}

TEST_CASE("worker world snapshots match retained chunks and clear old material geometry")
{
    using namespace TrainSim;
    const auto route = Sim::Route::NorthernValleyExpress();
    World::StreamingWorld retained(route);
    Render::WorldGeometry foreground;
    // Reuse both builders over countryside, stations, tunnel, viaduct and both ends.
    // Every vertex/index must match a separately owned worker route/world snapshot.
    auto scratch = std::make_unique<Render::WorldGeometry>();
    for (const auto& [distance,quality] : std::array<std::pair<double,int>,7>{
            {{52000,2},{52250,2},{68490,1},{123000,2},{178000,0},{286400,2},{0,2}}})
    {
        retained.update(distance);
        const double anchor=std::clamp((static_cast<double>(retained.currentChunk())+0.5)
                                      *retained.chunkLength(),0.0,route.lengthMetres());
        auto worker=std::async(std::launch::async,
            [copy=route,geometry=std::move(scratch),anchor,quality]() mutable {
                World::StreamingWorld snapshot(copy);
                snapshot.update(anchor);
                geometry->build(copy,snapshot,anchor,quality,750);
                return std::move(geometry);
            });
        foreground.build(route,retained,anchor,quality,750);
        scratch=worker.get();
        for (std::size_t material=0; material<Render::kWorldMaterialCount; ++material)
        {
            const auto& actual=scratch->meshes[material].data();
            const auto& expected=foreground.meshes[material].data();
            REQUIRE(actual.indices==expected.indices);
            REQUIRE(actual.vertices.size()==expected.vertices.size());
            for (std::size_t i=0; i<actual.vertices.size(); ++i)
            {
                REQUIRE(actual.vertices[i].position==expected.vertices[i].position);
                REQUIRE(actual.vertices[i].normal==expected.vertices[i].normal);
                REQUIRE(actual.vertices[i].textureCoordinate==expected.vertices[i].textureCoordinate);
                REQUIRE(actual.vertices[i].color==expected.vertices[i].color);
            }
        }
        REQUIRE(scratch->counters==foreground.counters);
        REQUIRE(scratch->segments==foreground.segments);
    }
}

TEST_CASE("verge tint varies by zone and joins the field headland exactly")
{
    using namespace TrainSim::Render;
    for (double side : {-1.0,1.0})
    {
        // The headland edge is the field margin colour used beyond 26 m.
        REQUIRE(ruralVergeTint(1234.5,side*26.0)==Microsoft::Xna::Framework::Color(91,133,72,255));
        // The cess is greyer (less green dominance) than the mown verge beside it.
        int cessGreen=0,mownGreen=0,cessRed=0,mownRed=0;
        for (int i=0;i<200;++i)
        {
            const double distance=50'000.0+i*3.7;
            const auto cess=ruralVergeTint(distance,side*3.4);
            const auto mown=ruralVergeTint(distance,side*6.0);
            cessGreen+=cess.getGProperty()-cess.getRProperty();
            mownGreen+=mown.getGProperty()-mown.getRProperty();
            cessRed+=cess.getRProperty();
            mownRed+=mown.getRProperty();
        }
        REQUIRE(cessGreen<mownGreen);
        REQUIRE(cessRed>mownRed);
        // Along the route the mown verge changes colour instead of repeating one flat tone.
        int minimum=255,maximum=0;
        for (int i=0;i<400;++i)
        {
            const int g=ruralVergeTint(80'000.0+i*2.5,side*6.0).getGProperty();
            minimum=std::min(minimum,g);
            maximum=std::max(maximum,g);
        }
        REQUIRE(maximum-minimum>=8);
        // Value noise is continuous: nearby samples differ only slightly.
        for (double across : {3.5,5.0,6.8,8.0,10.0,15.0,24.0})
        {
            const auto a=ruralVergeTint(7'000.0,side*across);
            const auto b=ruralVergeTint(7'000.05,side*(across+0.01));
            REQUIRE(std::abs(a.getGProperty()-b.getGProperty())<=3);
        }
    }
}

TEST_CASE("grid terrain cells keep the source surface and cover it once")
{
    using namespace TrainSim::Render;
    using Microsoft::Xna::Framework::Vector3;
    // A deliberately non-planar cell: corner c is raised.
    const std::array<Vector3,4> corners={Vector3{0,0,0},Vector3{0,0,50},Vector3{17.6f,3,50},
                                         Vector3{17.6f,0,0}};
    const std::array<float,6> u={0,0.2f,0.4f,0.6f,0.8f,1};
    const std::array<float,5> v={0,0.13f,0.5f,0.91f,1};
    MeshBuilder mesh;
    addGridTerrainCell(mesh,corners,std::span<const float>{u},std::span<const float>{v},
        [](float,float) { return Microsoft::Xna::Framework::Color(90,120,60,255); },
        [](float a,float b) { return Microsoft::Xna::Framework::Vector2{a,b}; });
    // Every vertex lies on one of the two source triangles (a,b,c) or (a,c,d).
    const auto onSurface=[&](const Vector3& p) {
        const float uu=p.Z/50.0f,vv=p.X/17.6f;
        const auto q=uu>=vv ? corners[0]*(1-uu)+corners[1]*(uu-vv)+corners[2]*vv
                            : corners[0]*(1-vv)+corners[2]*uu+corners[3]*(vv-uu);
        return std::abs(q.Y-p.Y)<1e-3f;
    };
    float area=0;
    const auto& data=mesh.data();
    for (const auto& vertex : data.vertices) REQUIRE(onSurface(vertex.position));
    for (std::size_t i=0;i+2<data.indices.size();i+=3)
    {
        const auto& a=data.vertices[data.indices[i]].position;
        const auto& b=data.vertices[data.indices[i+1]].position;
        const auto& c=data.vertices[data.indices[i+2]].position;
        // Clipped triangles keep the quad winding; centroids stay on the source surface.
        REQUIRE(Vector3::Cross(b-a,c-a).Y*Vector3::Cross(
            data.vertices[data.indices[1]].position-data.vertices[data.indices[0]].position,
            data.vertices[data.indices[2]].position-data.vertices[data.indices[0]].position).Y>0);
        REQUIRE(onSurface((a+b+c)*(1.0f/3.0f)));
        area+=0.5f*std::abs((b.X-a.X)*(c.Z-a.Z)-(c.X-a.X)*(b.Z-a.Z));
    }
    REQUIRE(std::abs(area-17.6f*50.0f)<0.5f);
}

TEST_CASE("verge flowers stay on the inner fence line in deterministic drifts")
{
    using namespace TrainSim::Render;
    int flowering=0,total=0;
    for (int side : {-1,1})
        for (double distance=10'000.0;distance<12'000.0;distance+=2.0)
        {
            const auto a=vergeFlowerPlacement(distance,side);
            const auto b=vergeFlowerPlacement(distance,side);
            REQUIRE(a.distance==b.distance);
            REQUIRE(a.lateral*side>=7.55);
            REQUIRE(a.lateral*side<=kFenceLateral-0.1);
            REQUIRE(std::abs(a.distance-distance)<=0.76);
            REQUIRE(a.height>0.35f);
            REQUIRE(a.height<0.69f);
            ++total;
            if (a.flowering) ++flowering;
        }
    // Drifts: neither every clump nor none of them flowers.
    REQUIRE(flowering>total/8);
    REQUIRE(flowering<total*7/8);
    MeshBuilder clump;
    auto placement=vergeFlowerPlacement(10'004.0,1);
    placement.flowering=true;
    addVergeFlowerClump(clump,{0,0,0},placement);
    float top=0;
    for (const auto& vertex : clump.data().vertices) top=std::max(top,vertex.position.Y);
    REQUIRE(top>=placement.height*1.0f);
    REQUIRE(top<0.8f);
}

TEST_CASE("cable trough units sit low behind the masts and field boundaries vary")
{
    using namespace TrainSim::Render;
    REQUIRE(kCableTroughLateral>4.3f+0.45f);
    REQUIRE(kCableTroughLateral+0.17f<6.75f);
    MeshBuilder trough;
    addCableTroughUnit(trough,Microsoft::Xna::Framework::Matrix::getIdentityProperty(),3);
    for (const auto& vertex : trough.data().vertices)
    {
        REQUIRE(vertex.position.Y<=0.12f);
        REQUIRE(std::abs(vertex.position.Z)<=kCableTroughUnitLength*0.5);
    }
    std::array<int,4> kinds{};
    for (std::int64_t parcel=0;parcel<400;++parcel)
        for (int side : {-1,1})
        {
            const auto layout=fieldBoundaryLayout(parcel,side);
            ++kinds[static_cast<std::size_t>(layout.kind)];
            REQUIRE(layout.outerLateral>=96.0f);
            REQUIRE(layout.outerLateral<=180.0f);
            REQUIRE(layout.gateLateral>=24.0f);
            REQUIRE(layout.gateLateral<=layout.outerLateral-24.0f);
            REQUIRE(layout.height>=0.8f);
            REQUIRE(layout.height<=1.35f);
        }
    for (int count : kinds) REQUIRE(count>60);
}

TEST_CASE("urban verge joins the street ground and the palisade stays see-through")
{
    using namespace TrainSim::Render;
    constexpr double length=286'400;
    for (double side : {-1.0,1.0})
    {
        REQUIRE(urbanVergeTint(2'000,side*kUrbanVergeEnd,length)
            ==terminalGroundTint(2'000,side*kUrbanVergeEnd,length));
        // The cess is gravel, not grass: red is not dominated by green near the ballast.
        const auto cess=urbanVergeTint(2'000,side*3.5,length);
        REQUIRE(cess.getRProperty()+8>=cess.getGProperty());
    }
    REQUIRE(kPalisadeLateral<14.8-0.3);   // Inside the near street kerb.
    REQUIRE(kPalisadeLateral>8.36+1.0);   // Clear of the parallel track's ballast toe.
    const auto straight=[](double distance,double lateral) {
        struct P { float x,y,z,yaw; };
        return P{static_cast<float>(lateral),0.0f,static_cast<float>(distance),0.0f};
    };
    MeshBuilder posts,span;
    addPalisadePost(posts,straight,10.0,kPalisadeLateral);
    addPalisadeSpan(span,straight,10.0,10.0+kPalisadePostSpacing,kPalisadeLateral,true);
    float top=0;
    for (const auto& vertex : span.data().vertices)
    {
        top=std::max(top,vertex.position.Y);
        REQUIRE(vertex.position.Z>=9.99f);
        REQUIRE(vertex.position.Z<=10.0f+static_cast<float>(kPalisadePostSpacing)+0.01f);
    }
    REQUIRE(top>2.0f);
    REQUIRE(top<2.1f);
    // Pales are 60 mm blades every 250 mm: most of the run stays open.
    REQUIRE(span.data().vertices.size()>40);
}

TEST_CASE("house styles vary palette storeys roofs and keep the footprint")
{
    using namespace TrainSim::Render;
    std::vector<unsigned> facades,roofs;
    int bungalows=0,porches=0,barns=0;
    std::array<int,4> chimneys{};
    for (std::uint32_t i=0;i<400;++i)
    {
        const auto style=houseStyle(treeShapeSeed(1'000.0+i*37.0,60.0,29U));
        const auto facade=style.facade.getPackedValueProperty();
        const auto roof=style.roof.getPackedValueProperty();
        if (std::find(facades.begin(),facades.end(),facade)==facades.end()) facades.push_back(facade);
        if (std::find(roofs.begin(),roofs.end(),roof)==roofs.end()) roofs.push_back(roof);
        if (style.bungalow) ++bungalows;
        if (style.porch) ++porches;
        if (style.barn) ++barns;
        ++chimneys[static_cast<std::size_t>(style.chimneys)];
        REQUIRE(style.doorBay>=-1);
        REQUIRE(style.doorBay<=1);
        const auto envelope=houseEnvelope(1.0f,style);
        REQUIRE(envelope.width==7.0f);
        REQUIRE(envelope.length==10.0f);
        REQUIRE(envelope.roofHeight>envelope.wallHeight+1.5f);
        REQUIRE(envelope.roofHeight<9.0f);
    }
    REQUIRE(facades.size()==7);
    REQUIRE(roofs.size()==5);
    REQUIRE(bungalows>40);
    REQUIRE(bungalows<140);
    REQUIRE(porches>80);
    REQUIRE(barns>120);
    for (int count : chimneys) REQUIRE(count>50);
    // Outbuildings stay inside their own reserved parcel beside the house.
    for (bool barn : {false,true})
    {
        MeshBuilder walls;
        addRuralOutbuilding(walls,walls,Microsoft::Xna::Framework::Matrix::getIdentityProperty(),1.0f,
                            barn,Color(200,190,170,255),Color(120,60,50,255));
        for (const auto& vertex : walls.data().vertices)
        {
            REQUIRE(std::abs(vertex.position.X)<=TrainSim::World::kHouseOutbuildingHalfWidth+0.3);
            REQUIRE(std::abs(vertex.position.Z)<=TrainSim::World::kHouseOutbuildingHalfLength+0.3);
            REQUIRE(vertex.position.Y<6.2f);
        }
    }
    REQUIRE(TrainSim::World::kHouseOutbuildingOffset-TrainSim::World::kHouseOutbuildingHalfLength>6.2);
    REQUIRE(TrainSim::World::houseHasOutbuilding(4U,0.1));
    REQUIRE(!TrainSim::World::houseHasOutbuilding(4U,0.5));
    REQUIRE(!TrainSim::World::houseHasOutbuilding(3U,0.1));
}

TEST_CASE("urban blocks draw palettes and balconies from a stable seed")
{
    using namespace TrainSim::Render;
    std::vector<unsigned> facades;
    std::array<int,4> patterns{};
    for (std::uint32_t i=0;i<400;++i)
    {
        const auto style=blockStyle(treeShapeSeed(2'000.0+i*41.0,-70.0,31U));
        const auto facade=style.facade.getPackedValueProperty();
        if (std::find(facades.begin(),facades.end(),facade)==facades.end()) facades.push_back(facade);
        ++patterns[static_cast<std::size_t>(style.balconyPattern)];
        REQUIRE(blockStyle(treeShapeSeed(2'000.0+i*41.0,-70.0,31U)).facade==style.facade);
    }
    REQUIRE(facades.size()==8);
    for (int count : patterns) REQUIRE(count>50);
    REQUIRE(!blockBayHasBalcony(0,1.4));
    REQUIRE(blockBayHasBalcony(1,1.4));
    REQUIRE(!blockBayHasBalcony(1,4.2));
    REQUIRE(blockBayHasBalcony(2,-4.2));
    REQUIRE(blockBayHasBalcony(3,-1.4));
}

TEST_CASE("station families have distinct canopies and rural valances clear the platform")
{
    using namespace TrainSim::Render;
    std::vector<unsigned> decks;
    for (auto family : {StationFacadeFamily::Rural,StationFacadeFamily::Urban,
                        StationFacadeFamily::Waterfront,StationFacadeFamily::Suburban})
    {
        const auto deck=canopyPalette(family,false).deck.getPackedValueProperty();
        if (std::find(decks.begin(),decks.end(),deck)==decks.end()) decks.push_back(deck);
        REQUIRE(canopyPalette(family,false).valance==(family==StationFacadeFamily::Rural));
    }
    REQUIRE(decks.size()==4);
    // The terminus and suburban stops keep the original scheme.
    REQUIRE(canopyPalette(StationFacadeFamily::Rural,true).deck==CanopyPalette{}.deck);
    MeshBuilder valance;
    const int boards=addCanopyValance(valance,Microsoft::Xna::Framework::Matrix::getIdentityProperty(),
                                      30.0f,2.30f,Color(214,206,178,255));
    REQUIRE(boards>=150);
    for (const auto& vertex : valance.data().vertices)
    {
        // Lowest point stays about 2.2 m above the 0.78 m platform surface.
        REQUIRE(vertex.position.Y>=2.99f);
        REQUIRE(vertex.position.Y<=3.32f);
        REQUIRE(std::abs(vertex.position.Z)<=15.0f);
    }
    MeshBuilder soffit;
    addCanopySoffit(soffit,Vector3::Zero,12.0f,0.0f,nullptr,canopyPalette(StationFacadeFamily::Rural,false));
    bool rural=false;
    for (const auto& vertex : soffit.data().vertices)
        rural=rural || vertex.color==canopyPalette(StationFacadeFamily::Rural,false).soffit.getPackedValueProperty();
    REQUIRE(rural);
}

TEST_CASE("driving cab sides are solid with one door window and bogie per side")
{
    using namespace TrainSim::Render;
    for (const float direction : {-1.0f,1.0f})
        for (const float side : {-1.0f,1.0f})
        {
            const float inner=direction*63.3f,nose=direction*72.8f;
            MeshBuilder mesh,glass;
            addCabBodyside(mesh,glass,side,direction,inner,nose,Color(206,212,212,255),
                           Color(32,102,121,255));
            // Glazing covers only the door drop-light and the side window near the nose.
            float glassArea=0;
            for (std::size_t i=0;i+3<glass.data().vertices.size();i+=24)
            {
                float y0=1e9f,y1=-1e9f,z0=1e9f,z1=-1e9f;
                for (std::size_t k=i;k<i+24 && k<glass.data().vertices.size();++k)
                {
                    const auto& p=glass.data().vertices[k].position;
                    y0=std::min(y0,p.Y); y1=std::max(y1,p.Y);
                    z0=std::min(z0,p.Z); z1=std::max(z1,p.Z);
                    REQUIRE((p.Z-nose)*direction<=0.01f);
                    REQUIRE((p.Z-nose)*direction>=-kCabDoorFromNose-0.01f);
                }
                glassArea+=(y1-y0)*(z1-z0);
            }
            REQUIRE(glassArea>1.5f);
            REQUIRE(glassArea<3.5f);   // The former pane was 7.1 m x 1.34 m = 9.5 m2.
            // The bodyside stays within the shell envelope and below the cantrail.
            int wheelVertices=0;
            for (const auto& vertex : mesh.data().vertices)
            {
                REQUIRE(std::abs(vertex.position.X)<=1.57f);
                REQUIRE(vertex.position.Y<=2.56f);
                REQUIRE((vertex.position.Z-std::min(inner,nose))>=-0.05f);
                REQUIRE((vertex.position.Z-std::max(inner,nose))<=0.05f);
                if (vertex.position.Y<0.38f && std::abs(vertex.position.X)>1.0f
                    && std::abs(vertex.position.X)<1.17f) ++wheelVertices;
            }
            REQUIRE(wheelVertices>0);
        }
}

TEST_CASE("printed plaque text is centred bounded and readable from its face")
{
    using namespace TrainSim::Render;
    for (bool mirrored : {false,true})
    {
        MeshBuilder text;
        addPrintedPlaqueText(text,"GANGWAY",2.67f,0.25f,mirrored,Color(196,226,224,255),0.3f,0.105f,0.70f);
        REQUIRE(!text.data().vertices.empty());
        float left=1e9f,right=-1e9f,top=-1e9f,bottom=1e9f;
        for (const auto& vertex : text.data().vertices)
        {
            left=std::min(left,vertex.position.X); right=std::max(right,vertex.position.X);
            top=std::max(top,vertex.position.Y); bottom=std::min(bottom,vertex.position.Y);
            REQUIRE(vertex.position.Z==0.25f);
        }
        REQUIRE(right-left<=0.70f);
        REQUIRE(std::abs((left+right)*0.5f-0.3f)<0.03f);
        REQUIRE(top<=2.671f);
        REQUIRE(2.67f-bottom<=0.13f);
        REQUIRE(2.67f-bottom>=0.09f);
        // Mirrored text faces the other way: its first triangle winds oppositely.
        const auto& v=text.data().vertices;
        const auto& i=text.data().indices;
        const float facing=Vector3::Cross(v[i[1]].position-v[i[0]].position,
                                          v[i[2]].position-v[i[0]].position).Z;
        REQUIRE((facing>0)==mirrored);
    }
}

TEST_CASE("mesh segments stay below the buffer budget and cover every triangle once")
{
    using namespace TrainSim::Render;
    MeshBuilder small;
    small.addBox({0,0,0},{1,1,1},Color(1,2,3,255));
    const auto whole=meshSegments(small.data());
    REQUIRE(whole.size()==1);
    REQUIRE((whole[0]==MeshSegment{0,small.data().vertices.size(),0,small.data().indices.size()}));
    REQUIRE(meshSegments(MeshData{}).empty());

    MeshBuilder large;
    for (int i=0;i<500;++i)
    {
        const float x=static_cast<float>(i);
        large.addBox({x,0,0},{x+0.5f,1,1},Color(1,2,3,255));
        large.addTriangle({x,2,0},{x+1,2,0},{x,3,0},Color(4,5,6,255));
    }
    constexpr std::size_t budget=100;
    const auto segments=meshSegments(large.data(),budget);
    REQUIRE(segments.size()>=large.data().vertices.size()/budget);
    std::size_t nextIndex=0;
    for (const auto& segment : segments)
    {
        REQUIRE(segment.vertexCount<=budget);
        REQUIRE(segment.firstIndex==nextIndex);
        REQUIRE(segment.indexCount%3==0);
        for (std::size_t i=segment.firstIndex;i<segment.firstIndex+segment.indexCount;++i)
        {
            const std::size_t index=large.data().indices[i];
            REQUIRE(index>=segment.firstVertex);
            REQUIRE(index<segment.firstVertex+segment.vertexCount);
        }
        nextIndex+=segment.indexCount;
    }
    REQUIRE(nextIndex==large.data().indices.size());
    // The production budget keeps one segment's vertices well under the 64 MiB profile limit.
    REQUIRE(kMaxMeshSegmentVertices*sizeof(LitColorVertex)<std::size_t{67'108'863});
    MeshData bad;
    bad.vertices.resize(400);
    bad.indices={0,1,399};
    bool rejected=false;
    try { (void)meshSegments(bad,100); }
    catch (const std::logic_error&) { rejected=true; }
    REQUIRE(rejected);
}

TEST_CASE("worker track batches match foreground builds and reuse their builders")
{
    using namespace TrainSim;
    const auto route = Sim::Route::NorthernValleyExpress();
    Render::TrackGeometry foreground;
    auto scratch = std::make_unique<Render::TrackGeometry>();
    // Countryside, a station throat with turnouts, both route ends and a quality change.
    for (const auto& [anchor,quality] : std::array<std::pair<double,int>,5>{
            {{52000,2},{63000,2},{0,2},{286400,1},{192500,0}}})
    {
        auto worker=std::async(std::launch::async,
            [copy=route,geometry=std::move(scratch),anchor,quality]() mutable {
                geometry->build(copy,anchor,quality);
                return std::move(geometry);
            });
        foreground.build(route,anchor,quality);
        scratch=worker.get();
        REQUIRE(scratch->anchor==anchor);
        REQUIRE(scratch->quality==quality);
        REQUIRE(scratch->kilometrePosts==foreground.kilometrePosts);
        REQUIRE(scratch->fastenerAssemblies==foreground.fastenerAssemblies);
        REQUIRE(scratch->turnoutPointMotors==foreground.turnoutPointMotors);
        REQUIRE(scratch->segments==foreground.segments);
        for (std::size_t material=0; material<Render::kTrackMaterialCount; ++material)
        {
            const auto& actual=scratch->materials()[material]->data();
            const auto& expected=foreground.materials()[material]->data();
            REQUIRE(!expected.empty());
            REQUIRE(actual.indices==expected.indices);
            REQUIRE(actual.vertices.size()==expected.vertices.size());
            for (std::size_t i=0; i<actual.vertices.size(); i+=97)
                REQUIRE(actual.vertices[i].position==expected.vertices[i].position);
            for (const auto& segment : scratch->segments[material])
                REQUIRE(segment.vertexCount<=Render::kMaxMeshSegmentVertices);
        }
    }
}

TEST_CASE("staged upload urgency spreads the remainder over the frames before a boundary")
{
    using TrainSim::Render::stagedUploadBytesPerFrame;
    REQUIRE(stagedUploadBytesPerFrame(0,100.0,1.0)==0);
    // 102 frames away, two frames of margin: one hundredth per frame.
    REQUIRE(stagedUploadBytesPerFrame(100'000,102.0,1.0)==1'000);
    // Faster travel leaves fewer frames, so each frame must carry more.
    REQUIRE(stagedUploadBytesPerFrame(100'000,102.0,4.0)>stagedUploadBytesPerFrame(100'000,102.0,1.0));
    // At or past the boundary, everything that remains is due now; direction does not matter.
    REQUIRE(stagedUploadBytesPerFrame(100'000,1.0,5.0)==100'000);
    REQUIRE(stagedUploadBytesPerFrame(100'000,-102.0,1.0)==1'000);
    // A stationary train has no deadline pressure beyond the minimum.
    REQUIRE(stagedUploadBytesPerFrame(100'000,50.0,0.0)<=3);
}

TEST_CASE("parallel chunk generation reproduces the sequential world window exactly")
{
    using namespace TrainSim;
    const auto route = Sim::Route::NorthernValleyExpress();
    Render::WorldGeometry sequential(1), parallel(4);
    for (const auto& [distance,quality] : std::array<std::pair<double,int>,4>{
            {{52000,2},{192400,2},{123000,1},{286400,0}}})
    {
        World::StreamingWorld world(route);
        world.update(distance);
        const double anchor=std::clamp((static_cast<double>(world.currentChunk())+0.5)
                                      *world.chunkLength(),0.0,route.lengthMetres());
        sequential.build(route,world,anchor,quality,750);
        parallel.build(route,world,anchor,quality,750);
        REQUIRE(parallel.counters==sequential.counters);
        REQUIRE(parallel.segments==sequential.segments);
        for (std::size_t material=0; material<Render::kWorldMaterialCount; ++material)
        {
            const auto& a=parallel.meshes[material].data();
            const auto& b=sequential.meshes[material].data();
            REQUIRE(a.indices==b.indices);
            REQUIRE(a.vertices.size()==b.vertices.size());
            for (std::size_t i=0; i<a.vertices.size(); ++i)
            {
                REQUIRE(a.vertices[i].position==b.vertices[i].position);
                REQUIRE(a.vertices[i].color==b.vertices[i].color);
                REQUIRE(a.vertices[i].textureCoordinate==b.vertices[i].textureCoordinate);
            }
        }
    }
}

TEST_CASE("oncoming vehicle meshes fit their coupled length and loading gauge")
{
    using namespace TrainSim;
    using namespace TrainSim::Render;
    for (std::size_t type = 0; type < static_cast<std::size_t>(World::OtherTrainType::Count);
         ++type)
    {
        const World::OtherTrainConsist consist =
            World::otherTrainConsist(static_cast<World::OtherTrainType>(type));
        for (const World::OtherTrainVehicle& vehicle : consist.vehicles)
        {
            MeshBuilder builder;
            addOtherTrainVehicle(builder,vehicle.kind);
            const MeshData mesh = builder.take();
            REQUIRE(!mesh.empty());
            REQUIRE(mesh.indices.size()%3 == 0);
            REQUIRE(mesh.indices.size() < 60'000);
            float minZ = 1e9f, maxZ = -1e9f, maxY = 0.0f;
            for (const LitColorVertex& v : mesh.vertices)
            {
                REQUIRE(std::abs(v.position.X) <= 1.50f);
                REQUIRE(v.position.Y >= -0.001f);
                REQUIRE(v.position.Y <= 5.87f);
                REQUIRE(std::abs(v.normal.LengthSquared()-1.0f) < 1e-3f);
                minZ = std::min(minZ,v.position.Z);
                maxZ = std::max(maxZ,v.position.Z);
                maxY = std::max(maxY,v.position.Y);
            }
            for (std::uint32_t index : mesh.indices) REQUIRE(index < mesh.vertices.size());
            // Vehicles fill but never overlap their coupled spacing (DMU couplers may protrude
            // by a few centimetres at the free cab end).
            const float half = static_cast<float>(vehicle.lengthMetres*0.5);
            REQUIRE(maxZ <= half+0.08f);
            REQUIRE(minZ >= -half-0.08f);
            REQUIRE(maxZ-minZ > static_cast<float>(vehicle.lengthMetres)-0.9f);
            REQUIRE(maxY > 2.8f);
        }
    }
    // Electric locomotives and the regional motor car reach the contact wire; others do not.
    const auto top = [](World::OtherVehicleKind kind) {
        MeshBuilder builder;
        addOtherTrainVehicle(builder,kind);
        float maxY = 0.0f;
        for (const LitColorVertex& v : builder.data().vertices) maxY = std::max(maxY,v.position.Y);
        return maxY;
    };
    REQUIRE(top(World::OtherVehicleKind::EmuMotorCar) > 5.8f);
    REQUIRE(top(World::OtherVehicleKind::IntercityLocomotive) > 5.8f);
    REQUIRE(top(World::OtherVehicleKind::FreightLocomotive) > 5.8f);
    REQUIRE(top(World::OtherVehicleKind::DieselLeadingCar) < 4.3f);
    REQUIRE(top(World::OtherVehicleKind::TankWagon) < 4.0f);
    REQUIRE(otherVehicleIsFreightWagon(World::OtherVehicleKind::OpenWagon));
    REQUIRE(!otherVehicleIsFreightWagon(World::OtherVehicleKind::FreightLocomotive));
    REQUIRE(otherVehicleIsLocomotive(World::OtherVehicleKind::FreightLocomotive));

    // Leading ends carry white headlights at +Z, trailing cab cars red tail lamps at -Z.
    const auto litAt = [](World::OtherVehicleKind kind,std::uint32_t colour,float sign) {
        MeshBuilder builder;
        addOtherTrainVehicle(builder,kind);
        for (const LitColorVertex& v : builder.data().vertices)
            if (v.color == colour && v.position.Z*sign > 8.0f) return true;
        return false;
    };
    const std::uint32_t white = Color(244,246,228,255).getPackedValueProperty();
    const std::uint32_t red = Color(214,38,34,255).getPackedValueProperty();
    REQUIRE(litAt(World::OtherVehicleKind::IntercityLocomotive,white,1.0f));
    REQUIRE(!litAt(World::OtherVehicleKind::IntercityLocomotive,white,-1.0f));
    REQUIRE(litAt(World::OtherVehicleKind::DieselLeadingCar,white,1.0f));
    REQUIRE(litAt(World::OtherVehicleKind::DieselTrailingCar,red,-1.0f));
    REQUIRE(!litAt(World::OtherVehicleKind::DieselTrailingCar,white,1.0f));
    REQUIRE(litAt(World::OtherVehicleKind::IntercityTailCoach,red,-1.0f));
    REQUIRE(!litAt(World::OtherVehicleKind::IntercityCoach,red,-1.0f));
}

TEST_CASE("the default clock and clear weather reproduce the fixed daylight exactly")
{
    using namespace TrainSim;
    using namespace TrainSim::Render;
    const SunPosition reference = sunPosition(Sim::kDefaultServiceStartHours);
    REQUIRE(reference.azimuthOffset == 0.0);
    REQUIRE(shadowCastingPosition(reference) == reference);
    for (double heading : {0.0,0.7,-2.1,3.0})
    {
        const Vector3 a = sunRayDirection(heading,reference), b = daylightDirection(heading);
        REQUIRE(a.X == b.X);
        REQUIRE(a.Y == b.Y);
        REQUIRE(a.Z == b.Z);
    }
    const Atmosphere air = atmosphere(Sim::kDefaultServiceStartHours,{});
    const ExteriorLighting clear = exteriorLighting(0.0f);
    REQUIRE(air.light.ambient == clear.ambient);
    REQUIRE(air.light.diffuse == clear.diffuse);
    REQUIRE(air.light.bounce == clear.bounce);
    REQUIRE(air.light.uplight == clear.uplight);
    REQUIRE(air.keyLight == reference);
    for (float tunnel : {0.0f,0.4f,1.0f})
    {
        const ExteriorLighting mixed = exteriorLighting(tunnel,air.light);
        const ExteriorLighting fixed = exteriorLighting(tunnel);
        REQUIRE(mixed.ambient == fixed.ambient);
        REQUIRE(mixed.diffuse == fixed.diffuse);
    }
    REQUIRE(air.fogStart == 360.0f);
    REQUIRE(air.fogEnd == 1'050.0f);
    const Color fog = daylightSkyColor(0);
    REQUIRE(air.fogColour == Vector3(static_cast<float>(fog.getRProperty())/255.0f,
                                     static_cast<float>(fog.getGProperty())/255.0f,
                                     static_cast<float>(fog.getBProperty())/255.0f));
    REQUIRE(air.shadowStrength == 1.0f);
    REQUIRE(air.direct == 1.0f);
    REQUIRE(air.night == 0.0f);
    REQUIRE(air.sunDisc == 1.0f);
    REQUIRE(air.cloudTint == Vector3::One);
    REQUIRE(air.sunTint == Vector3::One);
    REQUIRE(stationLighting(air).ambient == clear.ambient);

    // The generalised sky builder emits the original daylight sky vertex for vertex.
    MeshBuilder fixedSky, palettedSky;
    addDaylightSky(fixedSky,1'070.0f);
    addAtmosphereSky(palettedSky,1'070.0f,air.sky,Vector3{0.4f,0.0f,-0.3f});
    REQUIRE(fixedSky.data().indices == palettedSky.data().indices);
    REQUIRE(fixedSky.data().vertices.size() == palettedSky.data().vertices.size());
    for (std::size_t i = 0; i < fixedSky.data().vertices.size(); ++i)
    {
        REQUIRE(fixedSky.data().vertices[i].position == palettedSky.data().vertices[i].position);
        REQUIRE(fixedSky.data().vertices[i].color == palettedSky.data().vertices[i].color);
    }
}

TEST_CASE("the sun crosses the sky and night, dusk and weather change the atmosphere")
{
    using namespace TrainSim;
    using namespace TrainSim::Render;
    // Continuous elevation through the day and night; highest after midday, lowest after midnight.
    double previous = sunPosition(0.0).elevation;
    double highest = -10.0, lowest = 10.0, highHour = 0.0, lowHour = 0.0;
    for (double hours = 0.05; hours < 24.0; hours += 0.05)
    {
        const double elevation = sunPosition(hours).elevation;
        REQUIRE(std::abs(elevation-previous) < 0.03);
        if (elevation > highest) { highest = elevation; highHour = hours; }
        if (elevation < lowest) { lowest = elevation; lowHour = hours; }
        previous = elevation;
    }
    REQUIRE(highest > 1.2 && highest < 1.35);
    REQUIRE(std::abs(highHour-13.0) < 0.2);
    REQUIRE(lowest < -0.3);
    REQUIRE(std::abs(lowHour-1.0) < 0.2);
    REQUIRE(sunPosition(4.4).elevation < 0.0);
    REQUIRE(sunPosition(4.6).elevation > 0.0);
    REQUIRE(sunPosition(21.6).elevation < 0.0);
    for (double hours : {0.5,6.0,13.0,19.5,22.0})
    {
        const SunPosition cast = shadowCastingPosition(sunPosition(hours));
        REQUIRE(cast.elevation >= 25.0*3.14159265/180.0-1e-9);
        // The key light never shines up from below the horizon.
        const Atmosphere air = atmosphere(hours,{});
        REQUIRE(sunRayDirection(0.3,air.keyLight).Y < -0.3f);
    }

    const Atmosphere day = atmosphere(13.0,{});
    const Atmosphere dusk = atmosphere(21.35,{});
    const Atmosphere night = atmosphere(0.5,{});
    REQUIRE(night.night == 1.0f);
    REQUIRE(night.light.ambient.X < 0.12f && night.light.ambient.Z < 0.16f);
    REQUIRE(night.light.diffuse.Y < day.light.diffuse.Y*0.25f);
    REQUIRE(night.sky.stars > 0.9f);
    REQUIRE(night.sunDisc == 0.0f);
    REQUIRE(night.moonDisc > 0.9f);
    REQUIRE(night.sky.zenith.Z < 30.0f);
    REQUIRE(stationLighting(night).ambient.X > night.light.ambient.X*3.0f);
    REQUIRE(dusk.sky.glow.X > 10.0f);
    REQUIRE(dusk.night > 0.0f && dusk.night < 1.0f);
    // Golden light: warmer (relatively less blue) than midday.
    REQUIRE(dusk.light.diffuse.Z/dusk.light.diffuse.X < day.light.diffuse.Z/day.light.diffuse.X);
    REQUIRE(dusk.shadowStrength < 1.0f);
    REQUIRE(day.night == 0.0f);

    const Atmosphere overcast = atmosphere(13.0,Sim::weatherConditions(Sim::Weather::Overcast));
    REQUIRE(overcast.direct < 0.2f);
    REQUIRE(overcast.shadowStrength < 0.5f);
    REQUIRE(overcast.sunDisc == 0.0f);
    REQUIRE(overcast.light.diffuse.X < day.light.diffuse.X*0.2f);
    REQUIRE(overcast.light.ambient.X > day.light.ambient.X);
    // A grey sky: the zenith is far less blue-dominant than the clear one.
    REQUIRE(overcast.sky.zenith.Z-overcast.sky.zenith.X < 25.0f);
    const Atmosphere fog = atmosphere(13.0,Sim::weatherConditions(Sim::Weather::Fog));
    REQUIRE(fog.fogEnd < 300.0f);
    REQUIRE(fog.fogStart < 40.0f);
    const Atmosphere rain = atmosphere(13.0,Sim::weatherConditions(Sim::Weather::Rain));
    REQUIRE(rain.rain == 1.0f);
    REQUIRE(rain.fogEnd < 700.0f);
    REQUIRE(rain.light.ambient.X < overcast.light.ambient.X);
    REQUIRE(rain.cloudTint.X < overcast.cloudTint.X);

    // Sky palettes rebuild only on a visible change; stars appear only after dark.
    REQUIRE(!skyPaletteChanged(day.sky,atmosphere(13.01,{}).sky));
    REQUIRE(skyPaletteChanged(day.sky,night.sky));
    MeshBuilder starry, daySky;
    addAtmosphereSky(starry,1'070.0f,night.sky,Vector3::UnitX);
    addAtmosphereSky(daySky,1'070.0f,day.sky,Vector3::UnitX);
    REQUIRE(starry.data().vertices.size() == daySky.data().vertices.size()+320*4);
    for (const auto& v : starry.data().vertices) REQUIRE(v.position.Length() < 1'071.0f);
}

TEST_CASE("night glow overlays copy only matching glazing and lamps and lit rooms are stable")
{
    using namespace TrainSim::Render;
    MeshBuilder vehicle;
    vehicle.addBox(Vector3{-1,0,-1},Vector3{1,1,1},Color(43,79,96,255));      // Glass.
    vehicle.addBox(Vector3{-1,2,-1},Vector3{1,3,1},Color(188,52,48,255));     // Body.
    vehicle.addBox(Vector3{-0.1f,4,-0.1f},Vector3{0.1f,4.2f,0.1f},Color(214,38,34,255)); // Lamp.
    const std::array<NightGlowMatch,2> matches{{
        {Color(43,79,96,255),Color(214,198,156,255)},{Color(214,38,34,255),Color(255,44,30,255)}}};
    const MeshData glow = nightGlowOverlay(vehicle.data(),matches);
    // All six shaded faces of both matching boxes, none of the body.
    REQUIRE(glow.indices.size() == 2*6*2*3);
    for (const auto& v : glow.vertices)
    {
        REQUIRE(v.position.Y < 1.01f || v.position.Y > 3.9f);
        REQUIRE(v.color == Color(214,198,156,255).getPackedValueProperty()
                || v.color == Color(255,44,30,255).getPackedValueProperty());
    }
    // A lit room is a deterministic property of its seed, close to the intended two in five.
    int lit = 0;
    for (std::uint32_t seed = 0; seed < 20'000; ++seed)
    {
        lit += nightWindowLit(seed) ? 1 : 0;
        REQUIRE(nightWindowLit(seed) == nightWindowLit(seed));
    }
    REQUIRE(lit > 7'800 && lit < 9'000);
    MeshBuilder pool;
    addLightPool(pool,Vector3{2,1,3},5.0f,Color(90,70,40,255));
    REQUIRE(pool.data().indices.size() == 12*3);
    for (const auto& v : pool.data().vertices)
    {
        REQUIRE(std::abs(v.position.Y-1.0f) < 1e-6f);
        REQUIRE(Vector3::Distance(v.position,Vector3{2,1,3}) <= 5.0001f);
        REQUIRE(v.normal.Y > 0.99f);
    }
}

TEST_CASE("rain streaks stay outside the cars and window beads stay on their pane")
{
    using namespace TrainSim::Render;
    MeshBuilder rain;
    REQUIRE(addRainStreaks(rain,600,7U) == 600);
    REQUIRE(rain.data().indices.size() == 600*2*6);
    for (const auto& v : rain.data().vertices)
    {
        REQUIRE(std::abs(v.position.X) > kRainCorridorHalfWidth);
        REQUIRE(std::abs(v.position.X) < kRainTileHalfWidth+0.05f);
        REQUIRE(v.position.Z > -0.05f && v.position.Z < kRainTileLength+0.05f);
        REQUIRE(v.position.Y > -kRainTileHeight*0.5f-1.0f && v.position.Y < kRainTileHeight*0.5f);
    }
    MeshBuilder right;
    addRainStreaks(right,200,9U,1.0f);
    for (const auto& v : right.data().vertices) REQUIRE(v.position.X > kRainCorridorHalfWidth);
    MeshBuilder beads;
    const int added = addWindowDroplets(beads,1.4575f,0.88f,2.32f,-3.0f,-1.5f,11U);
    REQUIRE(added > 40);
    for (const auto& v : beads.data().vertices)
    {
        REQUIRE(v.position.X == 1.4575f);
        REQUIRE(v.position.Y > 0.86f && v.position.Y < 2.34f);
        REQUIRE(v.position.Z > -3.2f && v.position.Z < -1.45f);
    }
}

TEST_CASE("night glow joins the streamed world and lit windows survive a new anchor")
{
    using namespace TrainSim;
    const auto route = Sim::Route::NorthernValleyExpress();
    World::StreamingWorld world(route);
    world.update(192'400.0);
    Render::WorldGeometry a(1), b(1);
    const double anchor = (static_cast<double>(world.currentChunk())+0.5)*world.chunkLength();
    a.build(route,world,anchor,2,750);
    b.build(route,world,anchor+250.0,2,750);
    const auto glow = static_cast<std::size_t>(Render::WorldMaterial::NightGlow);
    REQUIRE(a.meshes[glow].data().indices.size() > 3'000);
    // Same window set, different anchor: the lit selection must not reshuffle.
    REQUIRE(a.meshes[glow].data().indices.size() == b.meshes[glow].data().indices.size());
    // A low evening sun bakes different projected shadows but the same night glow.
    Render::WorldGeometry evening(1);
    evening.build(route,world,anchor,2,750,Render::shadowCastingPosition(Render::sunPosition(20.5)));
    REQUIRE(evening.meshes[glow].data().indices == a.meshes[glow].data().indices);
    const auto shadows = static_cast<std::size_t>(Render::WorldMaterial::Shadows);
    REQUIRE(evening.meshes[shadows].data().vertices.size()
            == a.meshes[shadows].data().vertices.size());
    bool moved = false;
    for (std::size_t i = 0; i < a.meshes[shadows].data().vertices.size() && !moved; ++i)
        moved = !(evening.meshes[shadows].data().vertices[i].position
                  == a.meshes[shadows].data().vertices[i].position);
    REQUIRE(moved);
}
