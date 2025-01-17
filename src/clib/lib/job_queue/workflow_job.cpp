#include <stdlib.h>

#include <ert/util/int_vector.hpp>

#include <ert/config/config_parser.hpp>

#include <ert/job_queue/job_kw_definitions.hpp>
#include <ert/job_queue/workflow_job.hpp>
#include <string>
#include <unordered_map>

using namespace std::string_literals;

/* The default values are interepreted as no limit. */
#define DEFAULT_INTERNAL false

#define INTERNAL_KEY "INTERNAL"
#define FUNCTION_KEY "FUNCTION"
#define SCRIPT_KEY "SCRIPT"

#define NULL_STRING "NULL"

struct workflow_job_struct {
    bool internal;
    int min_arg;
    int max_arg;
    /** Should contain values from the config_item_types enum in config.h.*/
    int_vector_type *arg_types;
    char *executable;
    char *internal_script_path;
    char *function;
    char *name;
    void *lib_handle;
    workflow_job_ftype *dl_func;
    bool valid;
};

bool workflow_job_internal(const workflow_job_type *workflow_job) {
    return workflow_job->internal;
}

const char *workflow_job_get_name(const workflow_job_type *workflow_job) {
    return workflow_job->name;
}

config_parser_type *workflow_job_alloc_config() {
    config_parser_type *config = config_alloc();
    {
        config_schema_item_type *item;

        item = config_add_schema_item(config, MIN_ARG_KEY, false);
        config_schema_item_set_argc_minmax(item, 1, 1);
        config_schema_item_iset_type(item, 0, CONFIG_INT);

        item = config_add_schema_item(config, MAX_ARG_KEY, false);
        config_schema_item_set_argc_minmax(item, 1, 1);
        config_schema_item_iset_type(item, 0, CONFIG_INT);

        item = config_add_schema_item(config, ARG_TYPE_KEY, false);
        config_schema_item_set_argc_minmax(item, 2, 2);
        config_schema_item_iset_type(item, 0, CONFIG_INT);

        stringlist_type *var_types = stringlist_alloc_new();
        stringlist_append_copy(var_types, JOB_STRING_TYPE);
        stringlist_append_copy(var_types, JOB_INT_TYPE);
        stringlist_append_copy(var_types, JOB_FLOAT_TYPE);
        stringlist_append_copy(var_types, JOB_BOOL_TYPE);
        config_schema_item_set_indexed_selection_set(item, 1, var_types);

        item = config_add_schema_item(config, EXECUTABLE_KEY, false);
        config_schema_item_set_argc_minmax(item, 1, 1);
        config_schema_item_iset_type(item, 0, CONFIG_EXECUTABLE);

        item = config_add_schema_item(config, SCRIPT_KEY, false);
        config_schema_item_set_argc_minmax(item, 1, 1);
        config_schema_item_iset_type(item, 0, CONFIG_PATH);

        item = config_add_schema_item(config, FUNCTION_KEY, false);
        config_schema_item_set_argc_minmax(item, 1, 1);

        item = config_add_schema_item(config, INTERNAL_KEY, false);
        config_schema_item_set_argc_minmax(item, 1, 1);
        config_schema_item_iset_type(item, 0, CONFIG_BOOL);
    }
    return config;
}

void workflow_job_update_config_compiler(const workflow_job_type *workflow_job,
                                         config_parser_type *config_compiler) {
    config_schema_item_type *item =
        config_add_schema_item(config_compiler, workflow_job->name, false);
    // Ensure that the arg_types mapping is at least as large as the
    // max_arg value. The arg_type vector will be left padded with
    // CONFIG_STRING values.
    {
        int iarg;
        config_schema_item_set_argc_minmax(item, workflow_job->min_arg,
                                           workflow_job->max_arg);
        for (iarg = 0; iarg < int_vector_size(workflow_job->arg_types); iarg++)
            config_schema_item_iset_type(item, iarg,
                                         (config_item_types)int_vector_iget(
                                             workflow_job->arg_types, iarg));
    }
}

workflow_job_type *workflow_job_alloc(const char *name, bool internal) {
    workflow_job_type *workflow_job =
        (workflow_job_type *)util_malloc(sizeof *workflow_job);
    workflow_job->internal = internal; // this can not be changed run-time.
    workflow_job->min_arg = CONFIG_DEFAULT_ARG_MIN;
    workflow_job->max_arg = CONFIG_DEFAULT_ARG_MAX;
    workflow_job->arg_types = int_vector_alloc(0, CONFIG_STRING);

    workflow_job->executable = NULL;
    workflow_job->internal_script_path = NULL;
    workflow_job->function = NULL;

    if (name == NULL)
        util_abort(
            "%s: trying to create workflow_job with name == NULL - illegal\n",
            __func__);
    else
        workflow_job->name = util_alloc_string_copy(name);

    workflow_job->valid = false;

    return workflow_job;
}

void workflow_job_set_executable(workflow_job_type *workflow_job,
                                 const char *executable) {
    workflow_job->executable =
        util_realloc_string_copy(workflow_job->executable, executable);
}

char *workflow_job_get_executable(workflow_job_type *workflow_job) {
    return workflow_job->executable;
}

void workflow_job_set_internal_script(workflow_job_type *workflow_job,
                                      const char *script_path) {
    workflow_job->internal_script_path = util_realloc_string_copy(
        workflow_job->internal_script_path, script_path);
}

char *
workflow_job_get_internal_script_path(const workflow_job_type *workflow_job) {
    return workflow_job->internal_script_path;
}

bool workflow_job_is_internal_script(const workflow_job_type *workflow_job) {
    return workflow_job->internal && workflow_job->internal_script_path != NULL;
}

void workflow_job_set_function(workflow_job_type *workflow_job,
                               const char *function) {
    workflow_job->function =
        util_realloc_string_copy(workflow_job->function, function);
}

char *workflow_job_get_function(workflow_job_type *workflow_job) {
    return workflow_job->function;
}

void workflow_job_iset_argtype(workflow_job_type *workflow_job, int iarg,
                               config_item_types type) {
    if (type == CONFIG_STRING || type == CONFIG_INT || type == CONFIG_FLOAT ||
        type == CONFIG_BOOL)
        int_vector_iset(workflow_job->arg_types, iarg, type);
}

void workflow_job_set_min_arg(workflow_job_type *workflow_job, int min_arg) {
    workflow_job->min_arg = min_arg;
}

void workflow_job_set_max_arg(workflow_job_type *workflow_job, int max_arg) {
    workflow_job->max_arg = max_arg;
}

int workflow_job_get_min_arg(const workflow_job_type *workflow_job) {
    return workflow_job->min_arg;
}

int workflow_job_get_max_arg(const workflow_job_type *workflow_job) {
    return workflow_job->max_arg;
}

config_item_types
workflow_job_iget_argtype(const workflow_job_type *workflow_job, int index) {
    return (config_item_types)int_vector_safe_iget(workflow_job->arg_types,
                                                   index);
}

static void workflow_job_iset_argtype_string(workflow_job_type *workflow_job,
                                             int iarg, const char *arg_type) {
    config_item_types type = job_kw_get_type(arg_type);
    if (type != CONFIG_INVALID)
        workflow_job_iset_argtype(workflow_job, iarg, type);
}

static void workflow_job_validate_internal(workflow_job_type *workflow_job) {
    workflow_job->dl_func = nullptr;
    workflow_job->valid = false;

    if (workflow_job->executable != nullptr) {
        fprintf(stderr, "Must have executable == NULL for internal jobs\n");
        return;
    }

    if (workflow_job->internal_script_path && !workflow_job->function) {
        workflow_job->valid = true;
        return;
    }

    fprintf(stderr, "Must have function != NULL or internal_script != "
                    "NULL for internal jobs");
}

static void workflow_job_validate_external(workflow_job_type *workflow_job) {
    if (workflow_job->executable != NULL) {
        if (util_is_executable(workflow_job->executable))
            workflow_job->valid = true;
    }
}

static void workflow_job_validate(workflow_job_type *workflow_job) {
    if (workflow_job->internal)
        workflow_job_validate_internal(workflow_job);
    else
        workflow_job_validate_external(workflow_job);
}

workflow_job_type *workflow_job_config_alloc(const char *name,
                                             config_parser_type *config,
                                             const char *config_file) {
    workflow_job_type *workflow_job = NULL;
    config_content_type *content =
        config_parse(config, config_file, "--", NULL, NULL, NULL,
                     CONFIG_UNRECOGNIZED_WARN, true);
    if (config_content_is_valid(content)) {
        bool internal = DEFAULT_INTERNAL;
        if (config_content_has_item(content, INTERNAL_KEY))
            internal = config_content_iget_as_bool(content, INTERNAL_KEY, 0, 0);

        {
            workflow_job = workflow_job_alloc(name, internal);

            if (config_content_has_item(content, MIN_ARG_KEY))
                workflow_job_set_min_arg(
                    workflow_job,
                    config_content_iget_as_int(content, MIN_ARG_KEY, 0, 0));

            if (config_content_has_item(content, MAX_ARG_KEY))
                workflow_job_set_max_arg(
                    workflow_job,
                    config_content_iget_as_int(content, MAX_ARG_KEY, 0, 0));

            {
                int i;
                for (i = 0;
                     i < config_content_get_occurences(content, ARG_TYPE_KEY);
                     i++) {
                    int iarg =
                        config_content_iget_as_int(content, ARG_TYPE_KEY, i, 0);
                    const char *arg_type =
                        config_content_iget(content, ARG_TYPE_KEY, i, 1);

                    workflow_job_iset_argtype_string(workflow_job, iarg,
                                                     arg_type);
                }
            }

            if (config_content_has_item(content, FUNCTION_KEY))
                workflow_job_set_function(
                    workflow_job,
                    config_content_get_value(content, FUNCTION_KEY));

            if (config_content_has_item(content, EXECUTABLE_KEY))
                workflow_job_set_executable(
                    workflow_job, config_content_get_value_as_executable(
                                      content, EXECUTABLE_KEY));

            if (config_content_has_item(content, SCRIPT_KEY)) {
                workflow_job_set_internal_script(
                    workflow_job,
                    config_content_get_value_as_abspath(content, SCRIPT_KEY));
            }

            workflow_job_validate(workflow_job);

            if (!workflow_job->valid) {
                workflow_job_free(workflow_job);
                workflow_job = NULL;
            }
        }
    }
    config_content_free(content);
    return workflow_job;
}

void workflow_job_free(workflow_job_type *workflow_job) {
    free(workflow_job->function);
    free(workflow_job->executable);
    int_vector_free(workflow_job->arg_types);
    free(workflow_job->internal_script_path);
    free(workflow_job->name);
    free(workflow_job);
}

void workflow_job_free__(void *arg) {
    auto workflow_job = static_cast<workflow_job_type *>(arg);
    workflow_job_free(workflow_job);
}

/**
  The workflow job can return an arbitrary (void *) pointer. It is the
  calling scopes responsability to interpret this object correctly. If
  the the workflow job allocates storage the calling scope must
  discard it.
*/
static void *workflow_job_run_internal(const workflow_job_type *job, void *self,
                                       bool verbose,
                                       const stringlist_type *arg) {
    return job->dl_func(self, arg);
}

static void *workflow_job_run_external(const workflow_job_type *job,
                                       bool verbose,
                                       const stringlist_type *arg) {
    char **argv = stringlist_alloc_char_copy(arg);

    util_spawn_blocking(job->executable, stringlist_get_size(arg),
                        (const char **)argv, NULL, NULL);

    if (argv != NULL) {
        int i;
        for (i = 0; i < stringlist_get_size(arg); i++)
            free(argv[i]);
        free(argv);
    }
    return NULL;
}

/** This is the old C way and will only be used from the TUI */
void *workflow_job_run(const workflow_job_type *job, void *self, bool verbose,
                       const stringlist_type *arg) {
    if (job->internal) {
        if (workflow_job_is_internal_script(job)) {
            fprintf(stderr, "*** Can not run internal script workflow jobs "
                            "using this method: workflow_job_run()\n");
            return NULL;
        } else {
            return workflow_job_run_internal(job, self, verbose, arg);
        }
    } else {
        return workflow_job_run_external(job, verbose, arg);
    }
}
