#include "simulation.hpp"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;
namespace
{
constexpr double pi = 3.141592653589793238462643383279502884;
struct Field
{
    std::size_t nx, ny;
    std::vector<double> values;
    Field(std::size_t x, std::size_t y) : nx(x), ny(y), values(x * y) {}
    double& operator()(std::size_t x, std::size_t y) { return values[y * nx + x]; }
    double operator()(std::size_t x, std::size_t y) const { return values[y * nx + x]; }
};
std::size_t wrap(long long i, std::size_t n)
{
    const auto size = static_cast<long long>(n);
    return static_cast<std::size_t>((i % size + size) % size);
}
double mean(const Field& f)
{
    double s = 0.0;
    for (double v : f.values) s += v;
    return s / static_cast<double>(f.values.size());
}
struct Flow { Field psi, u, v; double poisson_residual = 0; Flow(std::size_t x, std::size_t y) : psi(x,y), u(x,y), v(x,y) {} };

bool power_of_two(std::size_t n) { return n && (n & (n - 1)) == 0; }
void fft(std::vector<std::complex<double>>& a, bool inverse)
{
    const std::size_t n=a.size();
    for(std::size_t i=1,j=0;i<n;++i)
    {
        std::size_t bit=n>>1;
        for(;j&bit;bit>>=1)j^=bit;
        j^=bit;
        if(i<j)std::swap(a[i],a[j]);
    }
    for(std::size_t len=2;len<=n;len<<=1)
    {
        const double angle=(inverse?2:-2)*pi/static_cast<double>(len);
        const std::complex<double> root(std::cos(angle),std::sin(angle));
        for(std::size_t i=0;i<n;i+=len)
        {
            std::complex<double> w(1,0);
            for(std::size_t j=0;j<len/2;++j)
            {
                const auto u=a[i+j],v=a[i+j+len/2]*w;
                a[i+j]=u+v; a[i+j+len/2]=u-v; w*=root;
            }
        }
    }
    if(inverse)for(auto& value:a)value/=static_cast<double>(n);
}

void spectral_poisson(const Field& omega,Field& psi,const SimulationConfig& c)
{
    using Complex=std::complex<double>;
    std::vector<Complex> work(omega.values.size());
    const double average=mean(omega);
    for(std::size_t y=0;y<c.ny;++y){std::vector<Complex> row(c.nx);for(std::size_t x=0;x<c.nx;++x)row[x]=omega(x,y)-average;fft(row,false);for(std::size_t x=0;x<c.nx;++x)work[y*c.nx+x]=row[x];}
    for(std::size_t x=0;x<c.nx;++x){std::vector<Complex> col(c.ny);for(std::size_t y=0;y<c.ny;++y)col[y]=work[y*c.nx+x];fft(col,false);for(std::size_t y=0;y<c.ny;++y)work[y*c.nx+x]=col[y];}
    const double dx=c.length_x/c.nx,dy=c.length_y/c.ny;
    for(std::size_t y=0;y<c.ny;++y)for(std::size_t x=0;x<c.nx;++x)
    {
        const double lambda=-4*std::pow(std::sin(pi*x/c.nx),2)/(dx*dx)-4*std::pow(std::sin(pi*y/c.ny),2)/(dy*dy);
        if(lambda==0)work[y*c.nx+x]=0; else work[y*c.nx+x]=-work[y*c.nx+x]/lambda;
    }
    for(std::size_t x=0;x<c.nx;++x){std::vector<Complex> col(c.ny);for(std::size_t y=0;y<c.ny;++y)col[y]=work[y*c.nx+x];fft(col,true);for(std::size_t y=0;y<c.ny;++y)work[y*c.nx+x]=col[y];}
    for(std::size_t y=0;y<c.ny;++y){std::vector<Complex> row(c.nx);for(std::size_t x=0;x<c.nx;++x)row[x]=work[y*c.nx+x];fft(row,true);for(std::size_t x=0;x<c.nx;++x)psi(x,y)=row[x].real();}
}

Flow solve_flow(const Field& w, const SimulationConfig& c)
{
    const double dx = c.length_x / c.nx, dy = c.length_y / c.ny;
    const double idx2 = 1.0/(dx*dx), idy2 = 1.0/(dy*dy);
    const double denom = 2.0*(idx2+idy2);
    const double omega = 1.85;
    Flow flow(c.nx,c.ny);
    Field rhs = w;
    const double wmean = mean(rhs);
    for (double& v : rhs.values) v -= wmean;
    if(power_of_two(c.nx)&&power_of_two(c.ny)) spectral_poisson(w,flow.psi,c);
    else for (std::size_t iteration=0; iteration<c.poisson_iterations; ++iteration)
    {
        for (std::size_t y=0; y<c.ny; ++y) for (std::size_t x=0; x<c.nx; ++x)
        {
            const double target = (idx2*(flow.psi(wrap(static_cast<long long>(x)-1,c.nx),y)+flow.psi(wrap(static_cast<long long>(x)+1,c.nx),y)) +
                                   idy2*(flow.psi(x,wrap(static_cast<long long>(y)-1,c.ny))+flow.psi(x,wrap(static_cast<long long>(y)+1,c.ny))) + rhs(x,y))/denom;
            flow.psi(x,y) += omega*(target-flow.psi(x,y));
        }
        if (iteration % 20 == 19 || iteration+1==c.poisson_iterations)
        {
            double residual=0;
            for (std::size_t y=0; y<c.ny; ++y) for (std::size_t x=0; x<c.nx; ++x)
            {
                const double lap = idx2*(flow.psi(wrap(static_cast<long long>(x)-1,c.nx),y)-2*flow.psi(x,y)+flow.psi(wrap(static_cast<long long>(x)+1,c.nx),y)) +
                                    idy2*(flow.psi(x,wrap(static_cast<long long>(y)-1,c.ny))-2*flow.psi(x,y)+flow.psi(x,wrap(static_cast<long long>(y)+1,c.ny)));
                residual = std::max(residual,std::abs(lap+rhs(x,y)));
            }
            flow.poisson_residual=residual;
            if (residual <= c.poisson_tolerance) break;
        }
    }
    if(power_of_two(c.nx)&&power_of_two(c.ny))
    {
        for(std::size_t y=0;y<c.ny;++y)for(std::size_t x=0;x<c.nx;++x)
        {
            const double lap=idx2*(flow.psi(wrap(static_cast<long long>(x)-1,c.nx),y)-2*flow.psi(x,y)+flow.psi(wrap(static_cast<long long>(x)+1,c.nx),y))+
                               idy2*(flow.psi(x,wrap(static_cast<long long>(y)-1,c.ny))-2*flow.psi(x,y)+flow.psi(x,wrap(static_cast<long long>(y)+1,c.ny)));
            flow.poisson_residual=std::max(flow.poisson_residual,std::abs(lap+rhs(x,y)));
        }
    }
    for (std::size_t y=0; y<c.ny; ++y) for (std::size_t x=0; x<c.nx; ++x)
    {
        flow.u(x,y)=(flow.psi(x,wrap(static_cast<long long>(y)+1,c.ny))-flow.psi(x,wrap(static_cast<long long>(y)-1,c.ny)))/(2*dy);
        flow.v(x,y)=-(flow.psi(wrap(static_cast<long long>(x)+1,c.nx),y)-flow.psi(wrap(static_cast<long long>(x)-1,c.nx),y))/(2*dx);
    }
    return flow;
}
double forcing(double x,double y,const SimulationConfig& c)
{
    return c.forcing_amplitude*std::sin(2*pi*x/c.length_x)*std::sin(2*pi*y/c.length_y);
}
Field rhs(const Field& w,const Flow& f,const SimulationConfig& c)
{
    const double dx=c.length_x/c.nx,dy=c.length_y/c.ny,idx2=1/(dx*dx),idy2=1/(dy*dy);
    Field out(c.nx,c.ny);
    for(std::size_t y=0;y<c.ny;++y) for(std::size_t x=0;x<c.nx;++x)
    {
        const auto xm=wrap(static_cast<long long>(x)-1,c.nx),xp=wrap(static_cast<long long>(x)+1,c.nx);
        const auto ym=wrap(static_cast<long long>(y)-1,c.ny),yp=wrap(static_cast<long long>(y)+1,c.ny);
        double wx,wy;
        if(c.advection_scheme=="central")
        { wx=(w(xp,y)-w(xm,y))/(2*dx); wy=(w(x,yp)-w(x,ym))/(2*dy); }
        else
        { wx=f.u(x,y)>=0?(w(x,y)-w(xm,y))/dx:(w(xp,y)-w(x,y))/dx;
          wy=f.v(x,y)>=0?(w(x,y)-w(x,ym))/dy:(w(x,yp)-w(x,y))/dy; }
        const double lap=idx2*(w(xp,y)-2*w(x,y)+w(xm,y))+idy2*(w(x,yp)-2*w(x,y)+w(x,ym));
        const double xx=(x+0.5)*dx, yy=(y+0.5)*dy;
        out(x,y)=-f.u(x,y)*wx-f.v(x,y)*wy+c.viscosity*lap+forcing(xx,yy,c);
    }
    return out;
}
void write_vtk(const fs::path& path,const Field& w,const Flow& flow,const SimulationConfig& c)
{
    std::ofstream out(path);
    out<<"# vtk DataFile Version 3.0\nProof Observatory deterministic periodic vorticity field\nASCII\nDATASET STRUCTURED_POINTS\nDIMENSIONS "<<c.nx<<' '<<c.ny<<" 1\nORIGIN 0 0 0\nSPACING "<<c.length_x/c.nx<<' '<<c.length_y/c.ny<<" 1\nPOINT_DATA "<<w.values.size()<<"\nSCALARS vorticity double 1\nLOOKUP_TABLE default\n"<<std::setprecision(17);
    for(double v:w.values) out<<v<<'\n';
    out<<"VECTORS velocity double\n";
    for(std::size_t y=0;y<c.ny;++y)for(std::size_t x=0;x<c.nx;++x)out<<flow.u(x,y)<<' '<<flow.v(x,y)<<" 0\n";
}
void write_svg(const fs::path& path,const Field& w)
{
    double scale=0; for(double v:w.values)scale=std::max(scale,std::abs(v)); if(scale==0)scale=1;
    std::ofstream out(path);
    out<<"<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 "<<w.nx<<' '<<w.ny<<"\" shape-rendering=\"crispEdges\"><rect width=\"100%\" height=\"100%\" fill=\"white\"/>";
    for(std::size_t y=0;y<w.ny;++y)for(std::size_t x=0;x<w.nx;++x)
    {
        const double q=std::min(1.0,std::abs(w(x,y))/scale); const int r=w(x,y)>=0?255:static_cast<int>(255*(1-q));
        const int b=w(x,y)<0?255:static_cast<int>(255*(1-q)); const int g=static_cast<int>(255*(1-q));
        out<<"<rect x=\""<<x<<"\" y=\""<<y<<"\" width=\"1\" height=\"1\" fill=\"rgb("<<r<<','<<g<<','<<b<<")\"/>";
    }
    out<<"</svg>\n";
}
struct Diagnostics { double max_w=0,energy=0,enstrophy=0,div=0,max_div=0,max_speed=0; };
Diagnostics diagnostics(const Field& w,const Flow& f,const SimulationConfig& c)
{
    Diagnostics d; const double dx=c.length_x/c.nx,dy=c.length_y/c.ny; double div2=0;
    for(std::size_t y=0;y<c.ny;++y)for(std::size_t x=0;x<c.nx;++x)
    {
        const double speed2=f.u(x,y)*f.u(x,y)+f.v(x,y)*f.v(x,y);
        d.max_speed=std::max(d.max_speed,std::sqrt(speed2)); d.max_w=std::max(d.max_w,std::abs(w(x,y)));
        d.energy+=0.5*speed2; d.enstrophy+=0.5*w(x,y)*w(x,y);
        const double div=(f.u(wrap(static_cast<long long>(x)+1,c.nx),y)-f.u(wrap(static_cast<long long>(x)-1,c.nx),y))/(2*dx)+
                         (f.v(x,wrap(static_cast<long long>(y)+1,c.ny))-f.v(x,wrap(static_cast<long long>(y)-1,c.ny)))/(2*dy);
        div2+=div*div; d.max_div=std::max(d.max_div,std::abs(div));
    }
    const double area=dx*dy; d.energy*=area; d.enstrophy*=area; d.div=std::sqrt(div2/w.values.size()); return d;
}
std::string json_quote(std::string_view s)
{
    std::string out="\"";
    for (const unsigned char c : s)
    {
        switch (c)
        {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20)
            {
                constexpr char digits[] = "0123456789abcdef";
                out += "\\u00";
                out += digits[c >> 4];
                out += digits[c & 0xf];
            }
            else out += static_cast<char>(c);
        }
    }
    out += '"';
    return out;
}
void validate(const SimulationConfig& c)
{
    if(c.nx<8||c.ny<8||c.nx>4096||c.ny>4096)throw std::invalid_argument("grid dimensions must be in [8,4096]");
    if(!std::isfinite(c.length_x)||!std::isfinite(c.length_y)||!std::isfinite(c.dt)||
       !std::isfinite(c.viscosity)||!std::isfinite(c.forcing_amplitude)||
       c.length_x<=0||c.length_y<=0||c.dt<=0||c.steps==0||c.output_every==0||c.viscosity<0)
        throw std::invalid_argument("domain, dt, steps, output interval, and viscosity must be valid finite values");
    if(c.stop_after_step>c.steps)throw std::invalid_argument("stop-after must not exceed the total step count");
    if(c.boundary!="periodic")throw std::invalid_argument("this solver currently supports periodic boundary conditions");
    if(c.initial_condition!="gaussian"&&c.initial_condition!="dipole"&&c.initial_condition!="shear")throw std::invalid_argument("initial condition must be gaussian, dipole, or shear");
    if(c.advection_scheme!="upwind"&&c.advection_scheme!="central")throw std::invalid_argument("advection scheme must be upwind or central");
    if(c.time_scheme!="rk2"&&c.time_scheme!="euler")throw std::invalid_argument("time scheme must be rk2 or euler");
}

bool same_checkpoint_config(const YAML::Node& saved, const SimulationConfig& c)
{
    return saved["nx"].as<std::size_t>() == c.nx && saved["ny"].as<std::size_t>() == c.ny &&
        saved["length_x"].as<double>() == c.length_x && saved["length_y"].as<double>() == c.length_y &&
        saved["dt"].as<double>() == c.dt && saved["viscosity"].as<double>() == c.viscosity &&
        saved["forcing_amplitude"].as<double>() == c.forcing_amplitude &&
        saved["output_every"].as<std::size_t>() == c.output_every &&
        saved["poisson_iterations"].as<std::size_t>() == c.poisson_iterations &&
        saved["poisson_tolerance"].as<double>() == c.poisson_tolerance &&
        saved["initial_condition"].as<std::string>() == c.initial_condition &&
        saved["advection_scheme"].as<std::string>() == c.advection_scheme &&
        saved["time_scheme"].as<std::string>() == c.time_scheme &&
        saved["boundary"].as<std::string>() == c.boundary;
}

void write_checkpoint(const fs::path& path, const Field& w, const SimulationConfig& c,
                      std::size_t completed_step)
{
    const fs::path temporary = path.string() + ".tmp";
    const fs::path backup = path.string() + ".previous";
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("cannot create checkpoint: " + temporary.string());
    out << std::setprecision(17)
        << "{\n  \"schema\": \"proof-observatory.ns2d-checkpoint.v1\",\n"
        << "  \"completed_step\": " << completed_step << ",\n"
        << "  \"config\": {\n"
        << "    \"nx\": " << c.nx << ", \"ny\": " << c.ny << ",\n"
        << "    \"length_x\": " << c.length_x << ", \"length_y\": " << c.length_y << ",\n"
        << "    \"dt\": " << c.dt << ", \"viscosity\": " << c.viscosity << ",\n"
        << "    \"forcing_amplitude\": " << c.forcing_amplitude << ",\n"
        << "    \"output_every\": " << c.output_every << ",\n"
        << "    \"poisson_iterations\": " << c.poisson_iterations << ",\n"
        << "    \"poisson_tolerance\": " << c.poisson_tolerance << ",\n"
        << "    \"initial_condition\": " << json_quote(c.initial_condition) << ",\n"
        << "    \"advection_scheme\": " << json_quote(c.advection_scheme) << ",\n"
        << "    \"time_scheme\": " << json_quote(c.time_scheme) << ",\n"
        << "    \"boundary\": " << json_quote(c.boundary) << "\n  },\n"
        << "  \"vorticity\": [\n";
    for (std::size_t i = 0; i < w.values.size(); ++i)
    {
        if (i) out << ",\n";
        out << w.values[i];
    }
    out << "\n  ]\n}\n";
    out.close();
    if (!out) throw std::runtime_error("failed writing checkpoint: " + temporary.string());

    std::error_code ec;
    fs::remove(backup, ec);
    ec.clear();
    if (fs::exists(path))
    {
        fs::rename(path, backup, ec);
        if (ec) throw std::runtime_error("cannot preserve previous checkpoint: " + ec.message());
    }
    fs::rename(temporary, path, ec);
    if (ec)
    {
        if (fs::exists(backup))
        {
            std::error_code restore_error;
            fs::rename(backup, path, restore_error);
        }
        throw std::runtime_error("cannot install checkpoint: " + ec.message());
    }
    fs::remove(backup, ec);
}

std::size_t read_checkpoint(const fs::path& path, Field& w, const SimulationConfig& c)
{
    YAML::Node checkpoint;
    try { checkpoint = YAML::LoadFile(path.string()); }
    catch (const YAML::Exception& e) { throw std::runtime_error("cannot read checkpoint: " + std::string(e.what())); }
    if (checkpoint["schema"].as<std::string>() != "proof-observatory.ns2d-checkpoint.v1")
        throw std::runtime_error("unsupported checkpoint schema");
    if (!same_checkpoint_config(checkpoint["config"], c))
        throw std::runtime_error("checkpoint configuration does not match this run");
    const auto values = checkpoint["vorticity"];
    if (!values.IsSequence() || values.size() != w.values.size())
        throw std::runtime_error("checkpoint vorticity size does not match the grid");
    for (std::size_t i = 0; i < w.values.size(); ++i)
    {
        w.values[i] = values[i].as<double>();
        if (!std::isfinite(w.values[i])) throw std::runtime_error("checkpoint contains a non-finite vorticity value");
    }
    return checkpoint["completed_step"].as<std::size_t>();
}

std::size_t last_diagnostics_step(const fs::path& path)
{
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot read existing diagnostics for restart");
    std::string line, last;
    std::getline(in, line); // header
    while (std::getline(in, line)) if (!line.empty()) last = line;
    if (last.empty()) throw std::runtime_error("existing diagnostics contain no checkpoint row");
    const auto comma = last.find(',');
    if (comma == std::string::npos) throw std::runtime_error("existing diagnostics have an invalid final row");
    return static_cast<std::size_t>(std::stoull(last.substr(0, comma)));
}
}

int run_simulation(const SimulationConfig& c,const fs::path& output_dir)
{
    validate(c);
    fs::create_directories(output_dir);
    const double dx=c.length_x/c.nx,dy=c.length_y/c.ny;
    Field w(c.nx,c.ny);
    std::size_t start_step=0;
    const fs::path checkpoint_path=output_dir/"checkpoint.json";
    const bool resumed=!c.restart_from.empty();

    if (resumed)
    {
        start_step=read_checkpoint(c.restart_from,w,c);
        if (start_step>c.steps) throw std::runtime_error("checkpoint step exceeds the requested total step count");
        if (c.stop_after_step!=0&&c.stop_after_step<=start_step)
            throw std::invalid_argument("stop-after must be greater than the checkpoint step");
    }
    else
    {
        for(std::size_t y=0;y<c.ny;++y)for(std::size_t x=0;x<c.nx;++x)
        {
            const double xx=(x+0.5)*dx,yy=(y+0.5)*dy;
            if(c.initial_condition=="shear") w(x,y)=std::sin(2*pi*yy/c.length_y);
            else
            {
                auto periodic_delta=[](double a,double b,double length){double d=std::abs(a-b);return std::min(d,length-d);};
                const double cx=c.length_x/2,cy=c.length_y/2;
                const double r2=std::pow(periodic_delta(xx,cx,c.length_x),2)+std::pow(periodic_delta(yy,cy,c.length_y),2);
                w(x,y)=std::exp(-r2/(0.12*0.12));
                if(c.initial_condition=="dipole")
                {
                    const double r2b=std::pow(periodic_delta(xx,cx+c.length_x/4,c.length_x),2)+std::pow(periodic_delta(yy,cy,c.length_y),2);
                    w(x,y)-=std::exp(-r2b/(0.12*0.12));
                }
            }
        }
    }

    const fs::path diagnostics_path=output_dir/"diagnostics.csv";
    const bool append=resumed&&fs::exists(diagnostics_path);
    if (append&&last_diagnostics_step(diagnostics_path)!=start_step)
        throw std::runtime_error("diagnostics do not end at the checkpoint step; refusing to mix run outputs");
    std::ofstream csv(diagnostics_path,append?std::ios::app:std::ios::trunc);
    if(!csv)throw std::runtime_error("cannot create diagnostics.csv");
    csv<<std::setprecision(17);
    if(!append)csv<<"step,time,max_abs_vorticity,kinetic_energy,enstrophy,divergence_rms,divergence_max,max_speed,cfl,poisson_residual\n";

    const double dt_diff= c.viscosity==0 ? std::numeric_limits<double>::infinity() :
        0.5/(c.viscosity*(1/(dx*dx)+1/(dy*dy)));
    double final_cfl=0;
    Flow flow=solve_flow(w,c);
    std::size_t last_output_step=append?start_step:std::numeric_limits<std::size_t>::max();
    auto output=[&](std::size_t step,double time){
        flow=solve_flow(w,c);
        const auto d=diagnostics(w,flow,c);
        final_cfl=c.dt*(d.max_speed/dx+d.max_speed/dy);
        csv<<step<<','<<time<<','<<d.max_w<<','<<d.energy<<','<<d.enstrophy<<','<<d.div<<','<<d.max_div<<','<<d.max_speed<<','<<final_cfl<<','<<flow.poisson_residual<<'\n';
        csv.flush();
        if(!csv)throw std::runtime_error("failed writing diagnostics.csv");
        write_vtk(output_dir/("field_"+std::to_string(step)+".vtk"),w,flow,c);
        write_svg(output_dir/("vorticity_"+std::to_string(step)+".svg"),w);
        last_output_step=step;
    };
    if (!append) output(start_step,start_step*c.dt);

    const std::size_t end_step=c.stop_after_step==0?c.steps:c.stop_after_step;
    if (end_step<start_step) throw std::invalid_argument("stop-after must not precede the checkpoint step");
    for(std::size_t step=start_step+1;step<=end_step;++step)
    {
        flow=solve_flow(w,c);
        Field k1=rhs(w,flow,c),next=w;
        if(c.time_scheme=="euler")
        {
            for(std::size_t i=0;i<w.values.size();++i)next.values[i]=w.values[i]+c.dt*k1.values[i];
        }
        else
        {
            for(std::size_t i=0;i<w.values.size();++i)next.values[i]=w.values[i]+c.dt*k1.values[i];
            Flow f2=solve_flow(next,c); Field k2=rhs(next,f2,c);
            for(std::size_t i=0;i<w.values.size();++i)next.values[i]=w.values[i]+0.5*c.dt*(k1.values[i]+k2.values[i]);
        }
        w=std::move(next);
        const bool checkpoint_due=c.checkpoint_every>0&&step%c.checkpoint_every==0;
        const bool stopping=step==end_step;
        if(step%c.output_every==0||step==c.steps||stopping||checkpoint_due)output(step,step*c.dt);
        if(checkpoint_due||stopping&&end_step<c.steps)write_checkpoint(checkpoint_path,w,c,step);
    }

    if (last_output_step!=end_step) output(end_step,end_step*c.dt);
    if (c.steps==start_step) { final_cfl=c.dt*(diagnostics(w,flow,c).max_speed/dx+diagnostics(w,flow,c).max_speed/dy); }
    const bool complete=end_step==c.steps;
    if (complete&&c.checkpoint_every>0&&end_step>0)
        write_checkpoint(checkpoint_path,w,c,end_step);
    const auto d=diagnostics(w,flow,c);
    std::ofstream report(output_dir/"run.json");
    report<<std::setprecision(17)
        <<"{\n  \"schema\": \"proof-observatory.simulation.v2\",\n"
        <<"  \"model\": \"2d_incompressible_vorticity_streamfunction\",\n"
        <<"  \"equation\": "<<json_quote(c.viscosity==0?"euler":"navier_stokes")<<",\n"
        <<"  \"interpretation\": \"numerical experiment; not a proof of regularity or singularity\",\n"
        <<"  \"complete\": "<<(complete?"true":"false")<<",\n"
        <<"  \"resumed\": "<<(resumed?"true":"false")<<",\n"
        <<"  \"resumed_from_step\": "<<start_step<<",\n"
        <<"  \"completed_step\": "<<end_step<<",\n"
        <<"  \"config\": {\n    \"nx\": "<<c.nx<<", \"ny\": "<<c.ny<<", \"length_x\": "<<c.length_x<<", \"length_y\": "<<c.length_y<<",\n"
        <<"    \"dt\": "<<c.dt<<", \"steps\": "<<c.steps<<", \"viscosity\": "<<c.viscosity<<", \"forcing_amplitude\": "<<c.forcing_amplitude<<",\n"
        <<"    \"checkpoint_every\": "<<c.checkpoint_every<<", \"stop_after_step\": "<<c.stop_after_step<<",\n"
        <<"    \"initial_condition\": "<<json_quote(c.initial_condition)<<", \"advection_scheme\": "<<json_quote(c.advection_scheme)<<", \"time_scheme\": "<<json_quote(c.time_scheme)<<", \"boundary\": "<<json_quote(c.boundary)<<"\n  },\n"
        <<"  \"diagnostics\": {\n    \"max_abs_vorticity\": "<<d.max_w<<", \"kinetic_energy\": "<<d.energy<<", \"enstrophy\": "<<d.enstrophy<<",\n"
        <<"    \"divergence_rms\": "<<d.div<<", \"divergence_max\": "<<d.max_div<<", \"poisson_residual\": "<<flow.poisson_residual<<",\n"
        <<"    \"cfl_advective\": "<<final_cfl<<", \"explicit_diffusion_dt_limit\": ";
    if(c.viscosity==0)report<<"null, \"within_diffusion_limit\": true, \"diffusion_stability_condition\": \"not_applicable_inviscid\"\n  },\n";
    else report<<dt_diff<<", \"within_diffusion_limit\": "<<(c.dt<=dt_diff?"true":"false")<<"\n  },\n";
    report
        <<"  \"outputs\": [\"diagnostics.csv\", \"field_<step>.vtk\", \"vorticity_<step>.svg\""
        <<(fs::exists(checkpoint_path)?", \"checkpoint.json\"":"")<<"]\n}\n";
    if(!report)throw std::runtime_error("failed writing run.json");
    std::cout<<"Simulation "<<(complete?"complete":"checkpointed")<<": "<<end_step<<"/"<<c.steps<<" steps, t="<<c.dt*end_step<<", max |omega|="<<d.max_w<<"\nOutput: "<<fs::absolute(output_dir).string()<<'\n';
    if(c.dt>dt_diff)std::cerr<<"WARNING: timestep exceeds the explicit diffusion stability limit "<<dt_diff<<"\n";
    return 0;
}
