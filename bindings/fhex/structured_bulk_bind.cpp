#include "context_bind.hpp"
#include <array>
#include <ghex/bulk_communication_object.hpp>
#include <ghex/structured/rma_range.hpp>
#include <ghex/structured/rma_range_generator.hpp>
#include <ghex/structured/regular/domain_descriptor.hpp>
#include <ghex/structured/regular/halo_generator.hpp>
#include <ghex/structured/regular/field_descriptor.hpp>
#include <ghex/structured/pattern.hpp>

// those are configurable at compile time
#include "ghex_defs.hpp"

using namespace gridtools::ghex::fhex;
using arch_type                 = ghex::cpu;
using domain_id_type            = int;

namespace gridtools {
    namespace ghex {
        namespace fhex {

            struct struct_field_descriptor {
                fp_type *data;
                int   offset[3];
                int  extents[3];
                int     halo[6];
                int periodic[3];
                int n_components;
                int layout;
            };

            using field_vector_type = std::vector<struct_field_descriptor>;
            struct struct_domain_descriptor {
                field_vector_type *fields;
                int id;
                int device_id;
                int  first[3];  // indices of the first LOCAL grid point, in global index space
                int   last[3];  // indices of the last LOCAL grid point, in global index space
                int gfirst[3];  // indices of the first GLOBAL grid point, (1,1,1) by default
                int  glast[3];  // indices of the last GLOBAL grid point (model dimensions)
            };

            // compare two fields to establish, if the same pattern can be used for both
            struct field_compare {
                bool operator()( const struct_field_descriptor& lhs, const struct_field_descriptor& rhs ) const
                {
                    if(lhs.halo[0] < rhs.halo[0]) return true; 
                    if(lhs.halo[0] > rhs.halo[0]) return false;
                    if(lhs.halo[1] < rhs.halo[1]) return true; 
                    if(lhs.halo[1] > rhs.halo[1]) return false;
                    if(lhs.halo[2] < rhs.halo[2]) return true; 
                    if(lhs.halo[2] > rhs.halo[2]) return false;
                    if(lhs.halo[3] < rhs.halo[3]) return true; 
                    if(lhs.halo[3] > rhs.halo[3]) return false;
                    if(lhs.halo[4] < rhs.halo[4]) return true; 
                    if(lhs.halo[4] > rhs.halo[4]) return false;
                    if(lhs.halo[5] < rhs.halo[5]) return true; 
                    if(lhs.halo[5] > rhs.halo[5]) return false;
        
                    if(lhs.periodic[0] < rhs.periodic[0]) return true; 
                    if(lhs.periodic[0] > rhs.periodic[0]) return false;
                    if(lhs.periodic[1] < rhs.periodic[1]) return true; 
                    if(lhs.periodic[1] > rhs.periodic[1]) return false;
                    if(lhs.periodic[2] < rhs.periodic[2]) return true; 
                    if(lhs.periodic[2] > rhs.periodic[2]) return false;

                    return false;
                }
            };


            using grid_type                 = ghex::structured::grid;
            using grid_detail_type          = ghex::structured::detail::grid<std::array<int, GHEX_DIMS>>;
            using domain_descriptor_type    = ghex::structured::regular::domain_descriptor<domain_id_type, GHEX_DIMS>;
            using pattern_type              = ghex::pattern_container<communicator_type, grid_detail_type, domain_id_type>;
            using communication_obj_type    = ghex::communication_object<communicator_type, grid_detail_type, domain_id_type>;
            using field_descriptor_type     = ghex::structured::regular::field_descriptor<fp_type, arch_type, domain_descriptor_type,2,1,0>;
            using pattern_field_type        = ghex::buffer_info<pattern_type::value_type, arch_type, field_descriptor_type>;
            using pattern_field_vector_type = std::pair<std::vector<std::unique_ptr<field_descriptor_type>>, std::vector<pattern_field_type>>;
            using pattern_map_type          = std::map<struct_field_descriptor, pattern_type, field_compare>;
            using bco_type                  = ghex::bulk_communication_object<ghex::structured::rma_range_generator, pattern_type, field_descriptor_type>;
            using exchange_handle_type      = bco_type::handle;
            using buffer_info_type          = bco_type::buffer_info_type<field_descriptor_type>;
            using halo_generator_type       = ghex::structured::regular::halo_generator<domain_id_type, GHEX_DIMS>;

            // a map of field descriptors to patterns
            static pattern_map_type field_to_pattern;

            // each bulk communication object can only be used with
            // one set of domains / fields. Since the fortran API
            // allows passing different exchange handles to the ghex_exchange
            // function, we have to check if the BCO and EH match.
            struct bco_wrapper {
                bco_type bco;
                void *eh;
            };
        }
    }
}

extern "C"
void ghex_struct_co_init(ghex::bindings::obj_wrapper **wco_ref, ghex::bindings::obj_wrapper *wcomm)
{
    if(nullptr == wcomm) return;   
    auto &comm = *ghex::bindings::get_object_ptr_unsafe<communicator_type>(wcomm);
    auto bco = gridtools::ghex::bulk_communication_object<
        gridtools::ghex::structured::rma_range_generator,
        pattern_type,
        field_descriptor_type
        > (comm);
    *wco_ref = new ghex::bindings::obj_wrapper(bco_wrapper{std::move(bco),nullptr});
}

extern "C"
void ghex_struct_domain_add_field(struct_domain_descriptor *domain_desc, struct_field_descriptor *field_desc)
{
    if(nullptr == domain_desc || nullptr == field_desc) return;
    if(nullptr == domain_desc->fields){
        domain_desc->fields = new field_vector_type();
    } else {
	// TODO: verify that periodicity and halos are the same for the added field
	const struct_field_descriptor &fd = domain_desc->fields->front();
	for(int i=0; i<6; i++)
	    if(fd.halo[i] != field_desc->halo[i]){
		std::cerr << "Bad halo definition: when constructing a bulk exchange domain, all fields must have the same halo and periodicity definition.";
		std::terminate();
	    }
	for(int i=0; i<3; i++)
	    if(fd.periodic[i] != field_desc->periodic[i]){
		std::cerr << "Bad periodicity definition: when constructing a bulk exchange domain, all fields must have the same halo and periodicity definition.";
		std::terminate();
	    }
    }
    domain_desc->fields->push_back(*field_desc);
}

extern "C"
void ghex_struct_domain_free(struct_domain_descriptor *domain_desc)
{
    if(nullptr == domain_desc) return;
    delete domain_desc->fields;
    domain_desc->fields = nullptr;
    domain_desc->id = -1;
    domain_desc->device_id = -1;
}

extern "C"
void* ghex_struct_exchange_desc_new(struct_domain_descriptor *domains_desc, int n_domains)
{

    if(0 == n_domains || nullptr == domains_desc) return nullptr;

    // Create all necessary patterns:
    //  1. make a vector of local domain descriptors
    //  2. identify unique <halo, periodic> pairs
    //  3. make a pattern for each pair
    //  4. for each field, compute the correct pattern(wrapped_field) objects

    // switch from fortran 1-based numbering to C
    std::array<int, 3> gfirst;
    gfirst[0] = domains_desc[0].gfirst[0]-1;
    gfirst[1] = domains_desc[0].gfirst[1]-1;
    gfirst[2] = domains_desc[0].gfirst[2]-1;

    std::array<int, 3> glast;
    glast[0] = domains_desc[0].glast[0]-1;
    glast[1] = domains_desc[0].glast[1]-1;
    glast[2] = domains_desc[0].glast[2]-1;

    std::vector<domain_descriptor_type> local_domains;
    for(int i=0; i<n_domains; i++){

        std::array<int, 3> first;
        first[0] = domains_desc[i].first[0]-1;
        first[1] = domains_desc[i].first[1]-1;
        first[2] = domains_desc[i].first[2]-1;

        std::array<int, 3> last;
        last[0] = domains_desc[i].last[0]-1;
        last[1] = domains_desc[i].last[1]-1;
        last[2] = domains_desc[i].last[2]-1;

        local_domains.emplace_back(domains_desc[i].id, first, last);
    }
   
    // a vector of `pattern(field)` objects
    pattern_field_vector_type pattern_fields;

    for(int i=0; i<n_domains; i++){

        if(nullptr == domains_desc[i].fields) continue;
        
        field_vector_type &fields = *(domains_desc[i].fields);
        for(auto field: fields){
            auto pit = field_to_pattern.find(field);
            if (pit == field_to_pattern.end()) {
                std::array<int, 3> &periodic = *((std::array<int, 3>*)field.periodic);
                std::array<int, 6> &halo = *((std::array<int, 6>*)field.halo);
                auto halo_generator = halo_generator_type(gfirst, glast, halo, periodic);
                pit = field_to_pattern.emplace(std::make_pair(std::move(field), 
                        ghex::make_pattern<grid_type>(*ghex_context, halo_generator, local_domains))).first;
            } 
            
            pattern_type &pattern = (*pit).second;
            std::array<int, 3> &offset  = *((std::array<int, 3>*)field.offset);
            std::array<int, 3> &extents = *((std::array<int, 3>*)field.extents);
            std::unique_ptr<field_descriptor_type> field_desc_uptr(
                new field_descriptor_type(ghex::wrap_field<arch_type,2,1,0>(local_domains[i], field.data, offset, extents)));
            auto ptr = field_desc_uptr.get();
            pattern_fields.first.push_back(std::move(field_desc_uptr));
            pattern_fields.second.push_back(pattern(*ptr));
        }
    }

    return new ghex::bindings::obj_wrapper(std::move(pattern_fields));
}

extern "C"
void *ghex_struct_exchange(ghex::bindings::obj_wrapper *cowrapper, ghex::bindings::obj_wrapper *ewrapper)
{
    if(nullptr == cowrapper || nullptr == ewrapper) return nullptr;
    bco_wrapper &bcowr                        = *ghex::bindings::get_object_ptr_unsafe<bco_wrapper>(cowrapper);
    bco_type    &bco                          = bcowr.bco;
    pattern_field_vector_type &pattern_fields = *ghex::bindings::get_object_ptr_unsafe<pattern_field_vector_type>(ewrapper);

    // first time call: build the bco
    if(!bcowr.eh) {
        for (auto it=pattern_fields.second.begin(); it!=pattern_fields.second.end(); ++it) {
            bco.add_field(*it);
        }
        // exchange the RMA handles before any other BCO can be created
        bco.init();
	bcowr.eh = ewrapper;
    } else {
        if(bcowr.eh != ewrapper){
	  std::cerr << "This RMA communication object was previously initialized and used with different fields. You have to create a new communication object for this new set of fields." << std::endl;
	  std::terminate();
        }
    }
    
    return new ghex::bindings::obj_wrapper(bco.exchange());
}

extern "C"
void ghex_struct_exchange_handle_wait(ghex::bindings::obj_wrapper **ehwrapper)
{
    if(nullptr == *ehwrapper) return;
    exchange_handle_type &hex = *ghex::bindings::get_object_ptr_unsafe<exchange_handle_type>(*ehwrapper);
    hex.wait();
}
