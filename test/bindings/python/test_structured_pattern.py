#
# ghex-org
#
# Copyright (c) 2014-2023, ETH Zurich
# All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause
#
import pytest
import numpy as np
#import cupy as cp

from fixtures.context import *

from ghex.util.architecture import architecture
import ghex.structured
import ghex.structured.regular as regular
from ghex.structured.grid import index_space, unit_range


@pytest.mark.mpi_skip
def test_pattern(capsys, mpi_cart_comm):
    comm = ghex.mpi_comm(mpi_cart_comm)
    ctx = ghex.context(comm, True)

    #Nx, Ny, Nz = 2*260, 260, 80
    Nx, Ny, Nz = 2 * 260, 260, 1
    halos = ((2, 1), (1, 1), (0, 0))
    periodicity = (True, True, False)

    p_coord = tuple(mpi_cart_comm.Get_coords(mpi_cart_comm.Get_rank()))
    global_grid = index_space.from_sizes(Nx, Ny, Nz)
    sub_grids = global_grid.decompose(mpi_cart_comm.dims)
    sub_grid = sub_grids[p_coord]  # sub-grid in global coordinates
    owned_indices = sub_grid.subset["definition"]
    sub_grid.add_subset("halo", owned_indices.extend(*halos).without(owned_indices))

    memory_local_grid = sub_grid.translate(*(-origin_l for origin_l in sub_grid.bounds[0, 0, 0]))

    domain_desc = regular.domain_descriptor(ctx.rank(), owned_indices)
    halo_gen = regular.halo_generator(global_grid.subset["definition"], halos, periodicity)

    pattern = regular.make_pattern(ctx, halo_gen, [domain_desc])

    with capsys.disabled():
        print("python side: making co")
        print(pattern.grid_type)
        print(pattern.domain_id_type)

    co = regular.make_co(ctx, pattern)

    def make_field():
        field_1 = np.zeros(memory_local_grid.bounds.shape, dtype=np.float64, order='F') # todo: , order='F'
        #field_1 = cp.zeros(memory_local_grid.bounds.shape, dtype=np.float64, order='F')
        gfield_1 = regular.field_descriptor(domain_desc,
                                   field_1,
                                   memory_local_grid.subset["definition"][0, 0, 0],
                                   memory_local_grid.bounds.shape)#,
                                   #arch=architecture.CPU)
        return field_1, gfield_1

    field_1, gfield_1 = make_field()
    field_2, gfield_2 = make_field()
    field_3, gfield_3 = make_field()
    fields = (field_1, field_2, field_3)
    gfields = (gfield_1, gfield_2, gfield_3)
    for p_dim, p_coord_l in enumerate(p_coord):
        fields[p_dim][:, :, :] = p_coord_l

    res = co.exchange([pattern(gfields[0]),
                      pattern(gfields[1]),
                      pattern(gfields[2])])
    res.wait()

    rank_field, grank_field = make_field()
    rank_field[:, :, :] = ctx.rank()
    #cp.cuda.Device(0).synchronize()
    res = co.exchange([pattern(grank_field)]) # arch, dtype. exchange of fields living on cpu+gpu possible
    res.wait()
    #cp.cuda.Device(0).synchronize()

    with capsys.disabled():
        print("post_ex:")
        print(rank_field[:, :, 0])

    for m_idx, local_idx in zip(memory_local_grid.bounds, sub_grid.bounds):
        value_owner_coord = tuple(int(fields[dim][m_idx]) for dim in range(0, 3))
        value_owner_rank = mpi_cart_comm.Get_cart_rank(value_owner_coord)
        if all(l >= 0 and l <= global_grid.subset["definition"][-1, -1, -1][d] for d, l in enumerate(local_idx)):
            assert (local_idx in sub_grids[value_owner_coord].subset["definition"])

            assert rank_field[m_idx] == value_owner_rank
    
