// Copyright (c) Lawrence Livermore National Security, LLC and other Conduit
// Project developers. See top-level LICENSE AND COPYRIGHT files for dates and
// other details. No copyright assignment is required to contribute to Conduit.

//-----------------------------------------------------------------------------
///
/// file: blueprint_baseline_helpers.hpp
///
//-----------------------------------------------------------------------------

#ifndef BLUEPRINT_BASELINE_HELPERS_HPP
#define BLUEPRINT_BASELINE_HELPERS_HPP

#include <conduit.hpp>
#include <conduit_node.hpp>
#include <conduit_blueprint.hpp>
#include <conduit_relay.hpp>
#include <conduit_relay_io_blueprint.hpp>

//-----------------------------------------------------------------------------
// -- begin baseline utility functions --
//-----------------------------------------------------------------------------
// NOTE: REQUIRES sep, baseline_dir(), test_name(), get_rank(), barrier()
//  are defined before inclusion.

//-----------------------------------------------------------------------------
#ifdef GENERATE_BASELINES
  #ifdef _WIN32
    #include <direct.h>
    inline void create_path(const std::string &path) { _mkdir(path.c_str()); }
  #else
    #include <sys/stat.h>
    #include <sys/types.h>
    inline void create_path(const std::string &path) { mkdir(path.c_str(), S_IRWXU); }
  #endif
#else
  inline void create_path(const std::string &) {}
#endif

//-----------------------------------------------------------------------------
inline std::string
baseline_file(const std::string &basename)
{
    std::string path(baseline_dir());
    int r = get_rank();
    if(r == 0)
        create_path(path);
    path += (sep + test_name());
    if(r == 0)
        create_path(path);
    path += (sep + basename + ".yaml");
    barrier();
    return path;
}

//-----------------------------------------------------------------------------
inline void
make_baseline(const std::string &filename, const conduit::Node &n)
{
    conduit::relay::io::save(n, filename, "yaml");
}

//-----------------------------------------------------------------------------
inline void
load_baseline(const std::string &filename, conduit::Node &n)
{
    conduit::relay::io::load(filename, "yaml", n);
}

//-----------------------------------------------------------------------------
inline bool
compare_baseline(const std::string &filename, const conduit::Node &n)
{
    const double tolerance = 1.e-6;
    conduit::Node baseline, info;
    conduit::relay::io::load(filename, "yaml", baseline);

    // Node::diff returns true if the nodes are different. We want not different.
    bool equal = !baseline.diff(n, info, tolerance, true);

    if(!equal)
    {
       const char *line = "*************************************************************";
       std::cout << "Difference!" << std::endl;
       std::cout << line << std::endl;
       info.print();
    }
    return equal;
}

//-----------------------------------------------------------------------------
inline bool
check_if_hdf5_enabled()
{
    conduit::Node io_protos;
    conduit::relay::io::about(io_protos["io"]);
    return io_protos["io/protocols/hdf5"].as_string() == "enabled";
}

//-----------------------------------------------------------------------------
inline void
save_node(const std::string &filename, const conduit::Node &mesh)
{
    conduit::relay::io::blueprint::save_mesh(mesh, filename + ".yaml", "yaml");
}

//-----------------------------------------------------------------------------
inline void
save_visit(const std::string &filename, const conduit::Node &n)
{
#ifdef GENERATE_BASELINES
    // NOTE: My VisIt only wants to read HDF5 root files for some reason.
    bool hdf5_enabled = check_if_hdf5_enabled();

    auto pos = filename.rfind("/");
    std::string fn(filename.substr(pos+1,filename.size()-pos-1));
    pos = fn.rfind(".");
    std::string fn_noext(fn.substr(0, pos));


    // Save all the domains to individual files.
    auto ndoms = conduit::blueprint::mesh::number_of_domains(n);
    if(ndoms < 1)
        return;
    char dnum[20];
    if(ndoms == 1)
    {
        sprintf(dnum, "%05d", 0);
        std::stringstream ss;
        ss << fn_noext << "." << dnum;

        if(hdf5_enabled)
            conduit::relay::io::save(n, ss.str() + ".hdf5", "hdf5");
        // VisIt won't read it:
        conduit::relay::io::save(n, ss.str() + ".yaml", "yaml");
    }
    else
    {
        for(size_t i = 0; i < ndoms; i++)
        {
            sprintf(dnum, "%05d", static_cast<int>(i));
            std::stringstream ss;
            ss << fn_noext << "." << dnum;

            if(hdf5_enabled)
                conduit::relay::io::save(n[i], ss.str() + ".hdf5", "hdf5");
            // VisIt won't read it:
            conduit::relay::io::save(n[i], ss.str() + ".yaml", "yaml");
        }
    }

    // Add index stuff to it so we can plot it in VisIt.
    conduit::Node root;
    if(ndoms == 1)
        conduit::blueprint::mesh::generate_index(n, "", ndoms, root["blueprint_index/mesh"]);
    else
        conduit::blueprint::mesh::generate_index(n[0], "", ndoms, root["blueprint_index/mesh"]);
    root["protocol/name"] = "hdf5";
    root["protocol/version"] = CONDUIT_VERSION;
    root["number_of_files"] = ndoms;
    root["number_of_trees"] = ndoms;
    root["file_pattern"] = (fn_noext + ".%05d.hdf5");
    root["tree_pattern"] = "/";

    if(hdf5_enabled)
        conduit::relay::io::save(root, fn_noext + "_hdf5.root", "hdf5");

    root["file_pattern"] = (fn_noext + ".%05d.yaml");
    // VisIt won't read it:
    conduit::relay::io::save(root, fn_noext + "_yaml.root", "yaml");
#endif
}

//-----------------------------------------------------------------------------
// -- end baseline utility functions --
//-----------------------------------------------------------------------------

#endif
