// Copyright (c) Lawrence Livermore National Security, LLC and other Conduit
// Project developers. See top-level LICENSE AND COPYRIGHT files for dates and
// other details. No copyright assignment is required to contribute to Conduit.

//-----------------------------------------------------------------------------
///
/// file: conduit_blueprint_mpi_mesh_parmetis.cpp
///
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// conduit includes
//-----------------------------------------------------------------------------
#include "conduit_blueprint_mesh.hpp"
#include "conduit_blueprint_mesh_utils.hpp"
#include "conduit_blueprint_mpi_mesh.hpp"
#include "conduit_blueprint_mpi_mesh_parmetis.hpp"
#include "conduit_blueprint_o2mrelation.hpp"
#include "conduit_blueprint_o2mrelation_iterator.hpp"

#include "conduit_relay_mpi.hpp"

#include <parmetis.h>

//-----------------------------------------------------------------------------
// -- begin conduit --
//-----------------------------------------------------------------------------
namespace conduit
{

//-----------------------------------------------------------------------------
// -- begin conduit::blueprint --
//-----------------------------------------------------------------------------
namespace blueprint
{

//-----------------------------------------------------------------------------
// -- begin conduit::blueprint::mpi --
//-----------------------------------------------------------------------------
namespace mpi
{

//-----------------------------------------------------------------------------
// -- begin conduit::blueprint::mesh --
//-----------------------------------------------------------------------------

namespace mesh 
{
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-- Map Parmetis Types (idx_t and real_t) to conduit dtype ids 
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

// check our assumptions
static_assert(IDXTYPEWIDTH != 32 || IDXTYPEWIDTH != 64,
              "Metis idx_t is not 32 or 64 bits");

static_assert(REALTYPEWIDTH != 32 || REALTYPEWIDTH != 64,
              "Metis real_t is not 32 or 64 bits");

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// IDXTYPEWIDTH and REALTYPEWIDTH are metis type defs
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
index_t
metis_idx_t_to_conduit_dtype_id()
{
#if IDXTYPEWIDTH == 64
// 64 bits
// int64
    return conduit::DataType::INT64_ID;
#else
// 32 bits
// int32
    return conduit::DataType::INT32_ID;
#endif
}

//-----------------------------------------------------------------------------
index_t
metis_real_t_t_to_conduit_dtype_id()
{
#if REALTYPEWIDTH == 64
// 64 bits
// float64
    return conduit::DataType::FLOAT64_ID;
#else
// 32 bits
// float32
    return conduit::DataType::FLOAT32_ID;
#endif
}


//-----------------------------------------------------------------------------
// NOTE: this is generally useful, it should be added to mpi::mesh
//
// supported options:
//   topology: {string}
//   field_prefix: {string}
void generate_global_element_and_vertex_ids(conduit::Node &mesh,
                                            const Node &options,
                                            MPI_Comm comm)
{
    // TODO: Check of dest fields already exist, if they do error
    
    
    int par_rank = conduit::relay::mpi::rank(comm);
    int par_size = conduit::relay::mpi::size(comm);

    index_t local_num_doms = ::conduit::blueprint::mesh::number_of_domains(mesh);
    index_t global_num_doms = number_of_domains(mesh,comm);

    if(global_num_doms == 0)
    {
        return;
    }

    std::vector<Node*> domains;
    ::conduit::blueprint::mesh::domains(mesh,domains);

    // parse options
    std::string topo_name = "";
    std::string field_prefix = "";
    if( options.has_child("topology") )
    {
        topo_name = options["topology"].as_string();
    }
    else
    {
        // TOOD: IMP find the first topo name on a rank with data
        // for now, just grab first topo
        const Node &dom_topo = domains[0]->fetch("topologies")[0];
        topo_name = dom_topo.name();
    }

    if( options.has_child("field_prefix") )
    {
        field_prefix = options["field_prefix"].as_string();
    }

    // count all local elements + verts and create offsets
    uint64 local_total_num_eles=0;
    uint64 local_total_num_verts=0;

    Node local_info;
    // per domain local info books
    local_info["num_verts"].set(DataType::uint64(local_num_doms));
    local_info["num_eles"].set(DataType::uint64(local_num_doms));
    local_info["verts_offsets"].set(DataType::uint64(local_num_doms));
    local_info["eles_offsets"].set(DataType::uint64(local_num_doms));

    uint64_array local_num_verts = local_info["num_verts"].value();
    uint64_array local_num_eles  = local_info["num_eles"].value();

    uint64_array local_vert_offsets = local_info["verts_offsets"].value();
    uint64_array local_ele_offsets  = local_info["eles_offsets"].value();


    for(size_t local_dom_idx=0; local_dom_idx < domains.size(); local_dom_idx++)
    {
        Node &dom = *domains[local_dom_idx];
        // we do need to make sure we have the requested topo
        if(dom["topologies"].has_child(topo_name))
        {
            // get the topo node
            const Node &dom_topo = dom["topologies"][topo_name];
            // get the number of elements in the topo
            local_num_eles[local_dom_idx] = blueprint::mesh::utils::topology::length(dom_topo);
            local_ele_offsets[local_dom_idx] = local_total_num_eles;
            local_total_num_eles += local_num_eles[local_dom_idx];

            // get the coordset that the topo refs
            const Node &dom_cset = dom["coordsets"][dom_topo["coordset"].as_string()];
            // get the number of points in the coordset
            // THIS RETURNS ZERO:
            //local_num_verts[local_dom_idx] = blueprint::mesh::utils::coordset::length(dom_cset);
            // so we are using this: 
            local_num_verts[local_dom_idx] = dom_cset["values/x"].dtype().number_of_elements();
            local_vert_offsets[local_dom_idx] = local_total_num_verts;
            local_total_num_verts += local_num_verts[local_dom_idx];
        }
    }

    // calc per MPI task offsets using 
    // local_total_num_verts
    // local_total_num_eles

    // first count verts
    Node max_local, max_global;
    max_local.set(DataType::uint64(par_size));
    max_global.set(DataType::uint64(par_size));

    uint64_array max_local_vals = max_local.value();
    uint64_array max_global_vals = max_global.value();

    max_local_vals[par_rank] = local_total_num_verts;

    relay::mpi::max_all_reduce(max_local, max_global, comm);


    index_t global_verts_offset = 0;
    for(index_t i=0; i< par_rank; i++ )
    {
        global_verts_offset += max_global_vals[i];
    }
    
    // reset our buffers 
    for(index_t i=0; i< par_size; i++ )
    {
        max_local_vals[i]  = 0;
        max_global_vals[i] = 0;
    }

    // now count eles
    max_local_vals[par_rank] = local_total_num_eles;

    relay::mpi::max_all_reduce(max_local, max_global, comm);

    index_t global_eles_offset = 0;
    for(index_t i=0; i< par_rank; i++ )
    {
        global_eles_offset += max_global_vals[i];
    }

    // we now have our offsets, we can create output fields on each local domain
     for(size_t local_dom_idx=0; local_dom_idx < domains.size(); local_dom_idx++)
     {
         Node &dom = *domains[local_dom_idx];
         // we do need to make sure we have the requested topo
         if(dom["topologies"].has_child(topo_name))
         {
             Node &verts_field = dom["fields"][field_prefix + "global_vertex_ids"];
             verts_field["association"] = "vertex";
             verts_field["topology"] = topo_name;
             verts_field["values"].set(DataType::int64(local_num_verts[local_dom_idx]));

             int64 vert_base_idx = global_verts_offset + local_vert_offsets[local_dom_idx];

             int64_array vert_ids_vals = verts_field["values"].value();
             for(uint64 i=0; i < local_num_verts[local_dom_idx]; i++)
             {
                 vert_ids_vals[i] = i + vert_base_idx;
             }

             // NOTE: VISIT BP DOESNT SUPPORT UINT64!!!!
             Node &eles_field = dom["fields"][field_prefix + "global_element_ids"];
             eles_field["association"] = "element";
             eles_field["topology"] = topo_name;
             eles_field["values"].set(DataType::int64(local_num_eles[local_dom_idx]));

             int64 ele_base_idx = global_eles_offset + local_ele_offsets[local_dom_idx];

             int64_array ele_ids_vals = eles_field["values"].value();
             for(uint64 i=0; i < local_num_eles[local_dom_idx]; i++)
             {
                ele_ids_vals[i] = i + ele_base_idx;
             }
         }
    }
}

//-----------------------------------------------------------------------------
void generate_partition_field(conduit::Node &mesh,
                              MPI_Comm comm)
{
    Node opts;
    generate_partition_field(mesh,opts,comm);
}

//-----------------------------------------------------------------------------
void generate_partition_field(conduit::Node &mesh,
                              const conduit::Node &options,
                              MPI_Comm comm)
{
    generate_global_element_and_vertex_ids(mesh,
                                           options,
                                           comm);

    int par_rank = conduit::relay::mpi::rank(comm);
    int par_size = conduit::relay::mpi::size(comm);

    index_t global_num_doms = number_of_domains(mesh,comm);

    if(global_num_doms == 0)
    {
        return;
    }

    std::vector<Node*> domains;
    ::conduit::blueprint::mesh::domains(mesh,domains);

    // parse options
    std::string topo_name = "";
    std::string field_prefix = "";
    idx_t nparts = (idx_t)global_num_doms;

    if( options.has_child("topology") )
    {
        topo_name = options["topology"].as_string();
    }
    else
    {
        // TOOD: IMP find the first topo name on a rank with data
        // for now, just grab first topo
        const Node &dom_topo = domains[0]->fetch("topologies")[0];
        topo_name = dom_topo.name();
    }
    idx_t ncommonnodes;
    if ( options.has_child("parmetis_ncommonnodes") )
    {
        ncommonnodes = options["parmetis_ncommonnodes"].as_int();
    }
    else
    {
        // in 2D, zones adjacent if they share 2 nodes (edge)
        // in 3D, zones adjacent if they share 3 nodes (plane)
        std::string coordset_name
            = domains[0]->fetch(std::string("topologies/") + topo_name + "/coordset").as_string();
        const Node& coordset = domains[0]->fetch(std::string("coordsets/") + coordset_name);
        ncommonnodes = conduit::blueprint::mesh::coordset::dims(coordset);
    }

    if( options.has_child("field_prefix") )
    {
        field_prefix = options["field_prefix"].as_string();
    }

    if( options.has_child("partitions") )
    {
        nparts = (idx_t) options["partitions"].to_int64();
    }
    // TODO: Should this be an error or use default (discuss more)?
    // else
    // {
    //     CONDUIT_ERROR("Missing required option in generate_partition_field(): "
    //                   << "expected \"partitions\" field with number of partitions.");
    // }

    // we now have global element and vertex ids
    // we just need to do some counting and then 
    //  traverse our topo to convert this info to parmetis input

    // we need the total number of local eles
    // the total number of element to vers entries

    index_t local_total_num_eles =0;
    index_t local_total_ele_to_verts_size = 0;
    
    for(size_t local_dom_idx=0; local_dom_idx < domains.size(); local_dom_idx++)
    {
        Node &dom = *domains[local_dom_idx];
        // we do need to make sure we have the requested topo
        if(dom["topologies"].has_child(topo_name))
        {
            // get the topo node
            const Node &dom_topo = dom["topologies"][topo_name];
            // get the number of elements in the topo
            local_total_num_eles += blueprint::mesh::utils::topology::length(dom_topo);

            Node topo_offsets;
            blueprint::mesh::topology::unstructured::generate_offsets(dom_topo, topo_offsets);

            // TODO:
            // for unstructured we need to do shape math, for unif/rect/struct
            //  we need to do implicit math

            // for unstructured poly: 
            // add up all the sizes, don't use offsets?
            uint64_accessor sizes_vals = dom_topo["elements/sizes"].as_uint64_accessor();
            for(index_t i=0; i < sizes_vals.number_of_elements();i++)
            {
               local_total_ele_to_verts_size += sizes_vals[i];
            }

        }
    }

    // reminder:
    // idx_t eldist[] = {0, 3, 4};
    //
    // idx_t eptr[] = {0,4,8,12};
    //
    // idx_t eind[] = {0,1,3,4,
    //                        1,2,4,5,
    //                        3,4,6,7};

    Node parmetis_params;
    // eldist tells how many elements there are per mpi task,
    // it will be size par_size + 1
    parmetis_params["eldist"].set(DataType(metis_idx_t_to_conduit_dtype_id(),
                                           par_size+1));
    // eptr holds the offsets to the start of each element's
    // vertex list
    // size == total number of local elements (we counted this above)
    parmetis_params["eptr"].set(DataType(metis_idx_t_to_conduit_dtype_id(),
                                         local_total_num_eles+1));
    // eind holds, for each element, a list of vertex ids
    // (we also counted this above)
    parmetis_params["eind"].set(DataType(metis_idx_t_to_conduit_dtype_id(),
                                         local_total_ele_to_verts_size));

    // output array, size of local num elements
    parmetis_params["part"].set(DataType(metis_idx_t_to_conduit_dtype_id(),
                                         local_total_num_eles));


    // first lets get eldist setup:
    // eldist[0] = 0,\
    // eldist[1] == # of elements on rank 0-
    // eldist[2] == # of elemens on rank 0 + rank 1
    //    ...
    // eldist[n] == # of total elements
    //
    Node el_counts;
    el_counts["local"]  = DataType(metis_idx_t_to_conduit_dtype_id(),
                                   par_size);
    el_counts["global"] = DataType(metis_idx_t_to_conduit_dtype_id(),
                                   par_size);

    idx_t *el_counts_local_vals  = el_counts["local"].value();
    idx_t *el_counts_global_vals = el_counts["global"].value();
    el_counts_local_vals[par_rank] = local_total_num_eles;
    relay::mpi::max_all_reduce(el_counts["local"], el_counts["global"], comm);

    // prefix sum to set eldist
    idx_t *eldist_vals = parmetis_params["eldist"].value();
    eldist_vals[0] = 0;
    for(size_t i=0;i<(size_t)par_size;i++)
    {
        eldist_vals[i+1] =  eldist_vals[i] + el_counts_global_vals[i];
    }

    idx_t *eptr_vals = parmetis_params["eptr"].value();
    idx_t *eind_vals = parmetis_params["eind"].value();

    // now elptr  == prefix sum of the sizes
    // (note: the offsets don't matter for elptr b/c we are creating a compact
    //  rep for parmetis)
    //
    // and eind == look up of global vertex id
    size_t eptr_idx=0;
    size_t eind_idx=0;
    idx_t  curr_offset = 0;
    for(size_t local_dom_idx=0; local_dom_idx < domains.size(); local_dom_idx++)
    {
        Node &dom = *domains[local_dom_idx];
        // we do need to make sure we have the requested topo
        if(dom["topologies"].has_child(topo_name))
        {
            // get the topo node
            Node &dom_topo = dom["topologies"][topo_name];
            const Node &dom_g_vert_ids = dom["fields"][field_prefix + "global_vertex_ids"]["values"];

            // for unstruct poly: use sizes
            uint64_accessor sizes_vals = dom_topo["elements/sizes"].as_uint64_accessor();
            for(index_t i=0; i < sizes_vals.number_of_elements(); i++)
            {
                eptr_vals[eptr_idx] = curr_offset;
                curr_offset += sizes_vals[i];
                eptr_idx++;
            }
            // add last offset
            eptr_vals[eptr_idx] = curr_offset;

            int64_accessor global_vert_ids = dom_g_vert_ids.as_int64_accessor();
            // for each element:
            //   loop over each local vertex, and use global vert map to add and entry to eind

            uint64_accessor conn_vals = dom_topo["elements/connectivity"].as_uint64_accessor();

            o2mrelation::O2MIterator o2miter(dom_topo["elements"]);
            while(o2miter.has_next(conduit::blueprint::o2mrelation::ONE))
            {
                o2miter.next(conduit::blueprint::o2mrelation::ONE);
                o2miter.to_front(conduit::blueprint::o2mrelation::MANY);
                while(o2miter.has_next(conduit::blueprint::o2mrelation::MANY))
                {
                    o2miter.next(conduit::blueprint::o2mrelation::MANY);
                    const index_t local_vert_id = o2miter.index(conduit::blueprint::o2mrelation::DATA);
                    // get the conn
                    eind_vals[eind_idx] = (idx_t) global_vert_ids[conn_vals[local_vert_id]];
                    eind_idx++;
                }
            }
        }
    }

    idx_t wgtflag = 0; // weights are NULL
    idx_t numflag = 0; // C-style numbering
    idx_t ncon = 1; // the number of weights per vertex
    // equal weights for each proc
    std::vector<real_t> tpwgts(nparts, 1.0/nparts);
    real_t ubvec = 1.050000;

    // options == extra output
    idx_t parmetis_opts[] = {1,
                       PARMETIS_DBGLVL_TIME |
                       PARMETIS_DBGLVL_INFO |
                       PARMETIS_DBGLVL_PROGRESS |
                       PARMETIS_DBGLVL_REFINEINFO |
                       PARMETIS_DBGLVL_MATCHINFO |
                       PARMETIS_DBGLVL_RMOVEINFO |
                       PARMETIS_DBGLVL_REMAP,
                       0};
    // outputs
    idx_t edgecut = 0; // will hold # of cut edges

    // output array, size of local num elements
    parmetis_params["part"].set(DataType(metis_idx_t_to_conduit_dtype_id(),
                                         local_total_num_eles));
    idx_t *part_vals = parmetis_params["part"].value();

    int parmetis_res = ParMETIS_V3_PartMeshKway(eldist_vals,
                                                eptr_vals,
                                                eind_vals,
                                                NULL,
                                                &wgtflag,
                                                &numflag,
                                                &ncon,
                                                &ncommonnodes,
                                                &nparts,
                                                tpwgts.data(),
                                                &ubvec,
                                                parmetis_opts,
                                                &edgecut,
                                                part_vals,
                                                &comm);

    index_t part_vals_idx=0;
    // create output field with part result
    for(size_t local_dom_idx=0; local_dom_idx < domains.size(); local_dom_idx++)
    {
        Node &dom = *domains[local_dom_idx];
        // we do need to make sure we have the requested topo
        if(dom["topologies"].has_child(topo_name))
        {
            // get the topo node
            const Node &dom_topo = dom["topologies"][topo_name];
            // get the number of elements in the topo
            index_t dom_num_eles = blueprint::mesh::utils::topology::length(dom_topo);
            // for unstrcut we need to do shape math, for unif/rect/struct
            //  we need to do implicit math

            // NOTE: VISIT BP DOESNT SUPPORT UINT64!!!!
            Node &part_field = dom["fields"][field_prefix + "parmetis_result"];
            part_field["association"] = "element";
            part_field["topology"] = topo_name;
            part_field["values"].set(DataType::int64(dom_num_eles));

            int64_array part_field_vals = part_field["values"].value();
            for(index_t i=0; i < dom_num_eles; i++)
            {
               part_field_vals[i] = part_vals[part_vals_idx];
               part_vals_idx++;
            }
        }
    }

}


//-----------------------------------------------------------------------------
}
//-----------------------------------------------------------------------------
// -- end conduit::blueprint::mpi::mesh --
//-----------------------------------------------------------------------------


}
//-----------------------------------------------------------------------------
// -- end conduit::blueprint::mpi --
//-----------------------------------------------------------------------------


}
//-----------------------------------------------------------------------------
// -- end conduit::blueprint --
//-----------------------------------------------------------------------------

}
//-----------------------------------------------------------------------------
// -- end conduit:: --
//-----------------------------------------------------------------------------

