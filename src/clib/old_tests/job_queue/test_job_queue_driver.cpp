#include <stdlib.h>

#include <ert/util/test_util.hpp>
#include <ert/util/util.hpp>

#include <ert/job_queue/job_queue.hpp>
#include <ert/job_queue/lsf_driver.hpp>

#include <ert/job_queue/slurm_driver.hpp>
#include <ert/job_queue/torque_driver.hpp>

void job_queue_set_driver_(job_driver_type driver_type) {
    job_queue_type *queue = job_queue_alloc(10, "OK", "STATUS", "ERROR");
    queue_driver_type *driver = queue_driver_alloc(driver_type);

    job_queue_set_driver(queue, driver);

    job_queue_free(queue);
    queue_driver_free(driver);
}

void set_option_max_running_max_running_value_set() {
    queue_driver_type *driver_torque = queue_driver_alloc(TORQUE_DRIVER);
    test_assert_true(queue_driver_set_option(driver_torque, MAX_RUNNING, "42"));
    test_assert_string_equal("42", (const char *)queue_driver_get_option(
                                       driver_torque, MAX_RUNNING));
    queue_driver_free(driver_torque);

    queue_driver_type *driver_lsf = queue_driver_alloc(LSF_DRIVER);
    test_assert_true(queue_driver_set_option(driver_lsf, MAX_RUNNING, "72"));
    test_assert_string_equal(
        "72", (const char *)queue_driver_get_option(driver_lsf, MAX_RUNNING));
    queue_driver_free(driver_lsf);
}

void set_option_max_running_max_running_option_set() {
    queue_driver_type *driver_torque = queue_driver_alloc(TORQUE_DRIVER);
    test_assert_true(queue_driver_set_option(driver_torque, MAX_RUNNING, "42"));
    test_assert_string_equal("42", (const char *)queue_driver_get_option(
                                       driver_torque, MAX_RUNNING));
    queue_driver_free(driver_torque);
}

void set_option_invalid_option_returns_false() {
    queue_driver_type *driver_torque = queue_driver_alloc(TORQUE_DRIVER);
    test_assert_false(
        queue_driver_set_option(driver_torque, "MAKS_RUNNING", "42"));
    queue_driver_free(driver_torque);
}

void set_option_invalid_value_returns_false() {
    queue_driver_type *driver_torque = queue_driver_alloc(TORQUE_DRIVER);
    test_assert_false(
        queue_driver_set_option(driver_torque, "MAX_RUNNING", "2a"));
    queue_driver_free(driver_torque);
}

void set_option_valid_on_specific_driver_returns_true() {
    queue_driver_type *driver_torque = queue_driver_alloc(TORQUE_DRIVER);
    test_assert_true(
        queue_driver_set_option(driver_torque, TORQUE_NUM_CPUS_PER_NODE, "33"));
    test_assert_string_equal(
        "33", (const char *)queue_driver_get_option(driver_torque,
                                                    TORQUE_NUM_CPUS_PER_NODE));
    queue_driver_free(driver_torque);
}

void get_driver_option_lists() {
    //Torque driver option list
    {
        queue_driver_type *driver_torque = queue_driver_alloc(TORQUE_DRIVER);
        stringlist_type *option_list = stringlist_alloc_new();
        queue_driver_init_option_list(driver_torque, option_list);

        test_assert_true(stringlist_contains(option_list, MAX_RUNNING));
        test_assert_true(stringlist_contains(option_list, TORQUE_QSUB_CMD));
        test_assert_true(stringlist_contains(option_list, TORQUE_QSTAT_CMD));
        test_assert_true(
            stringlist_contains(option_list, TORQUE_QSTAT_OPTIONS));
        test_assert_true(stringlist_contains(option_list, TORQUE_QDEL_CMD));
        test_assert_true(stringlist_contains(option_list, TORQUE_QUEUE));
        test_assert_true(
            stringlist_contains(option_list, TORQUE_NUM_CPUS_PER_NODE));
        test_assert_true(stringlist_contains(option_list, TORQUE_NUM_NODES));
        test_assert_true(
            stringlist_contains(option_list, TORQUE_KEEP_QSUB_OUTPUT));
        test_assert_true(
            stringlist_contains(option_list, TORQUE_CLUSTER_LABEL));

        stringlist_free(option_list);
        queue_driver_free(driver_torque);
    }

    //Local driver option list (only general queue_driver options)
    {
        queue_driver_type *driver_local = queue_driver_alloc(LOCAL_DRIVER);
        stringlist_type *option_list = stringlist_alloc_new();
        queue_driver_init_option_list(driver_local, option_list);

        test_assert_true(stringlist_contains(option_list, MAX_RUNNING));

        stringlist_free(option_list);
        queue_driver_free(driver_local);
    }

    //Lsf driver option list
    {
        queue_driver_type *driver_lsf = queue_driver_alloc(LSF_DRIVER);
        stringlist_type *option_list = stringlist_alloc_new();
        queue_driver_init_option_list(driver_lsf, option_list);

        test_assert_true(stringlist_contains(option_list, MAX_RUNNING));
        test_assert_true(stringlist_contains(option_list, LSF_QUEUE));
        test_assert_true(stringlist_contains(option_list, LSF_RESOURCE));
        test_assert_true(stringlist_contains(option_list, LSF_SERVER));
        test_assert_true(stringlist_contains(option_list, LSF_RSH_CMD));
        test_assert_true(stringlist_contains(option_list, LSF_LOGIN_SHELL));
        test_assert_true(stringlist_contains(option_list, LSF_BSUB_CMD));
        test_assert_true(stringlist_contains(option_list, LSF_BJOBS_CMD));
        test_assert_true(stringlist_contains(option_list, LSF_BKILL_CMD));

        stringlist_free(option_list);
        queue_driver_free(driver_lsf);
    }

    //SLurm driver option list
    {
        queue_driver_type *driver_slurm = queue_driver_alloc(SLURM_DRIVER);
        stringlist_type *option_list = stringlist_alloc_new();
        queue_driver_init_option_list(driver_slurm, option_list);

        stringlist_fprintf(option_list, ", ", stdout);
        test_assert_true(stringlist_contains(option_list, MAX_RUNNING));
        test_assert_true(stringlist_contains(option_list, SLURM_SBATCH_OPTION));
        test_assert_true(
            stringlist_contains(option_list, SLURM_SCONTROL_OPTION));
        test_assert_true(stringlist_contains(option_list, SLURM_SQUEUE_OPTION));
        test_assert_true(
            stringlist_contains(option_list, SLURM_SCANCEL_OPTION));
        test_assert_true(
            stringlist_contains(option_list, SLURM_PARTITION_OPTION));
        test_assert_true(
            stringlist_contains(option_list, SLURM_SQUEUE_TIMEOUT_OPTION));
        test_assert_true(
            stringlist_contains(option_list, SLURM_MAX_RUNTIME_OPTION));
        test_assert_true(stringlist_contains(option_list, SLURM_MEMORY_OPTION));
        test_assert_true(
            stringlist_contains(option_list, SLURM_MEMORY_PER_CPU_OPTION));

        stringlist_free(option_list);
        queue_driver_free(driver_slurm);
    }
}

int main(int argc, char **argv) {
    job_queue_set_driver_(LSF_DRIVER);
    job_queue_set_driver_(LOCAL_DRIVER);
    job_queue_set_driver_(TORQUE_DRIVER);
    job_queue_set_driver_(SLURM_DRIVER);

    set_option_max_running_max_running_value_set();
    set_option_max_running_max_running_option_set();
    set_option_invalid_option_returns_false();
    set_option_invalid_value_returns_false();

    set_option_valid_on_specific_driver_returns_true();
    get_driver_option_lists();

    exit(0);
}
