/*
   Copyright (C) 2014  Equinor ASA, Norway.

   The file 'ert_run_context.h' is part of ERT - Ensemble based Reservoir Tool.

   ERT is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   ERT is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License at <http://www.gnu.org/licenses/gpl.html>
   for more details.
*/

#ifndef ERT_RUN_CONTEXT_H
#define ERT_RUN_CONTEXT_H

#include <ert/util/bool_vector.h>
#include <ert/util/type_macros.h>

#include <ert/res_util/path_fmt.hpp>

#include <ert/enkf/enkf_fs.hpp>
#include <ert/enkf/enkf_types.hpp>
#include <ert/enkf/run_arg.hpp>

#include <string>
#include <vector>

typedef struct ert_run_context_struct ert_run_context_type;
ert_run_context_type *ert_run_context_alloc_ENSEMBLE_EXPERIMENT(
    enkf_fs_type *sim_fs, std::vector<bool> active,
    std::vector<std::string> runpaths, std::vector<std::string> jobnames,
    int iter);

ert_run_context_type *
ert_run_context_alloc_INIT_ONLY(enkf_fs_type *sim_fs, init_mode_type init_mode,
                                std::vector<bool> iactive,
                                std::vector<std::string> runpaths, int iter);

ert_run_context_type *ert_run_context_alloc_SMOOTHER_RUN(
    enkf_fs_type *sim_fs, enkf_fs_type *target_update_fs,
    std::vector<bool> iactive, std::vector<std::string> runpaths,
    std::vector<std::string> jobnames, int iter);

extern "C" void ert_run_context_free(ert_run_context_type *);
extern "C" int ert_run_context_get_size(const ert_run_context_type *context);
run_mode_type ert_run_context_get_mode(const ert_run_context_type *context);
extern "C" int ert_run_context_get_iter(const ert_run_context_type *context);
extern "C" int ert_run_context_get_step1(const ert_run_context_type *context);
extern "C" run_arg_type *
ert_run_context_iget_arg(const ert_run_context_type *context, int index);
extern "C" void
ert_run_context_deactivate_realization(ert_run_context_type *context, int iens);
extern "C" const char *
ert_run_context_get_id(const ert_run_context_type *context);
extern "C" init_mode_type
ert_run_context_get_init_mode(const ert_run_context_type *context);
char *ert_run_context_alloc_run_id();

extern "C" enkf_fs_type *
ert_run_context_get_sim_fs(const ert_run_context_type *run_context);
extern "C" enkf_fs_type *
ert_run_context_get_update_target_fs(const ert_run_context_type *run_context);
extern "C" bool ert_run_context_iactive(const ert_run_context_type *context,
                                        int iens);

UTIL_IS_INSTANCE_HEADER(ert_run_context);

#endif