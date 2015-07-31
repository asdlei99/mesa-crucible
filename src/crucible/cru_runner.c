// Copyright 2015 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice (including the next
// paragraph) shall be included in all copies or substantial portions of the
// Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

/// \file
/// \brief Crucible Test Runner
///
/// The runner consists of two processes: master and slave. The master forks
/// the slave. The tests execute in the slave process. The master collects the
/// test results and prints their summary. The separation ensures that test
/// results and summary are printed even when a test crashes its process.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <crucible/cru_log.h>

#include "cru_runner.h"
#include "cru_test.h"
#include "cru_test_def.h"

typedef union cru_pipe cru_pipe_t;
typedef struct cru_slave cru_slave_t;
typedef struct cru_dispatch_packet cru_dispatch_packet_t;
typedef struct cru_result_packet cru_result_packet_t;

union cru_pipe {
    int fd[2];

    struct {
        int read_fd;
        int write_fd;
    };
};

/// Channel between the master process and a slave process.
struct cru_slave {
    /// \brief PID of the slave process.
    ///
    /// If -1, in the runner's master process before forking.
    /// If 0, in the runner's master process after forking.
    /// If > 0, in the runner's slave process.
    pid_t pid;

    uint32_t num_active_tests;

    cru_pipe_t dispatch_pipe;
    cru_pipe_t result_pipe;
};

struct cru_dispatch_packet {
    const cru_test_def_t *test_def;
};

struct cru_result_packet {
    const cru_test_def_t *test_def;
    cru_test_result_t result;
};

/// Document and assert that this is the runner's master process.
#define ASSERT_IN_MASTER_PROCESS(slave) \
    assert((slave)->pid == -1 || (slave)->pid > 0)

/// Document and assert that this is the runner's slave process.
#define ASSERT_IN_SLAVE_PROCESS(slave) \
    assert((slave)->pid == 0)

struct cru_master {
    cru_slave_t slaves[1];

    uint32_t num_tests;
    uint32_t num_pass;
    uint32_t num_fail;
    uint32_t num_skip;
};

static struct cru_master master = {
    .slaves = {
        { .pid = -1 },
    },
};

bool cru_runner_do_cleanup_phase = true;
bool cru_runner_do_image_dumps = false;
bool cru_runner_use_spir_v = false;

static bool
cru_pipe_init(cru_pipe_t *p)
{
    int err;

    err = pipe2(p->fd, O_CLOEXEC);
    if (err) {
        cru_loge("failed to create pipe");
        return false;
    }

    return true;
}

static void
cru_pipe_finish(cru_pipe_t *p)
{
    for (int i = 0; i < 2; ++i) {
        if (p->fd[i] != -1) {
            close(p->fd[i]);
        }
    }
}

static bool
cru_pipe_become_reader(cru_pipe_t *p)
{
    int err;

    assert(p->read_fd != -1);
    assert(p->write_fd != -1);

    err = close(p->write_fd);
    if (err == -1) {
        cru_loge("runner failed to close pipe's write fd");
        return false;
    }

    p->write_fd = -1;

    return true;
}

static bool
cru_pipe_become_writer(cru_pipe_t *p)
{
    int err;

    assert(p->read_fd != -1);
    assert(p->write_fd != -1);

    err = close(p->read_fd);
    if (err == -1) {
        cru_loge("runner failed to close pipe's read fd");
        return false;
    }

    p->read_fd = -1;

    return true;
}

static bool
cru_pipe_atomic_write_n(const cru_pipe_t *p, const void *data, size_t n)
{
    // POSIX.1-2001 says that writes of less than PIPE_BUF bytes are atomic.
    // We assume that, if all writes to a pipe are atomic, then all reads will
    // be atomic also.
    assert(n < PIPE_BUF);

    return write(p->write_fd, data, n) == n;
}

static bool
cru_pipe_atomic_read_n(const cru_pipe_t *p, void *data, size_t n)
{
    assert(n < PIPE_BUF);

    return read(p->read_fd, data, n) == n;
}

#define cru_pipe_atomic_write(p, data) \
    cru_pipe_atomic_write_n((p), (data), sizeof(*(data)))

#define cru_pipe_atomic_read(p, data) \
    cru_pipe_atomic_read_n((p), (data), sizeof(*(data)))

/// Return false if the pipe is empty or has errors.
static void
master_report_result(const cru_test_def_t *def, cru_test_result_t result)
{
    cru_log_tag(cru_test_result_to_string(result),
                "%s", def->name);
    fflush(stdout);

    switch (result) {
    case CRU_TEST_RESULT_PASS: master.num_pass++; break;
    case CRU_TEST_RESULT_FAIL: master.num_fail++; break;
    case CRU_TEST_RESULT_SKIP: master.num_skip++; break;
    }
}

/// Return false on failure.
static bool
master_send_dispatch(cru_slave_t *slave, const cru_test_def_t *def)
{
    ASSERT_IN_MASTER_PROCESS(slave);

    bool result = false;
    const cru_dispatch_packet_t pk = { .test_def = def };
    const struct sigaction ignore_sa = { .sa_handler = SIG_IGN };
    struct sigaction old_sa;
    int err;

    // If the slave process died, then writing to its dispatch pipe will emit
    // SIGPIPE. Ignore it, because the master should never die.
    err = sigaction(SIGPIPE, &ignore_sa, &old_sa);
    if (err == -1) {
        cru_loge("test runner failed to disable SIGPIPE");
        abort();
    }

    if (!cru_pipe_atomic_write(&slave->dispatch_pipe, &pk))
        goto cleanup;

    result = true;
    slave->num_active_tests += 1;

cleanup:
    err = sigaction(SIGPIPE, &old_sa, NULL);
    if (err == -1) {
        cru_loge("test runner failed to re-enable SIGPIPE");
        abort();
    }

    return result;
}

/// Return NULL if the pipe is empty or has errors.
static const cru_test_def_t *
slave_recv_dispatch(const cru_slave_t *slave)
{
    ASSERT_IN_SLAVE_PROCESS(slave);

    cru_dispatch_packet_t pk;

    if (!cru_pipe_atomic_read(&slave->dispatch_pipe, &pk))
        return false;

    return pk.test_def;
}

/// Return false if the pipe is empty or has errors.
static bool
master_recv_result(cru_slave_t *slave)
{
    ASSERT_IN_MASTER_PROCESS(slave);

    cru_result_packet_t pk;

    if (!cru_pipe_atomic_read(&slave->result_pipe, &pk))
        return false;

    slave->num_active_tests -= 1;
    master_report_result(pk.test_def, pk.result);

    return true;
}

/// Return false on failure.
static bool
slave_send_result(const cru_slave_t *slave,
                  const cru_test_def_t *def, cru_test_result_t result)
{
    ASSERT_IN_SLAVE_PROCESS(slave);

    const cru_result_packet_t pk = {
        .test_def = def,
        .result = result,
    };

    return cru_pipe_atomic_write(&slave->result_pipe, &pk);
}

static void
slave_run_test(const cru_slave_t *slave,
               const cru_test_def_t *def)
{
    ASSERT_IN_SLAVE_PROCESS(slave);

    cru_test_t *test;

    // The slave should not receive disabled tests.
    assert(def->priv.enable);

    test = cru_test_create(def);
    if (!test) {
        slave_send_result(slave, def, CRU_TEST_RESULT_FAIL);
        return;
    }

    if (cru_runner_do_image_dumps)
        cru_test_enable_dump(test);

    if (!cru_runner_do_cleanup_phase)
        cru_test_disable_cleanup(test);

    if (cru_runner_use_spir_v)
        cru_test_enable_spir_v(test);

    cru_test_start(test);
    cru_test_wait(test);
    slave_send_result(slave, def, cru_test_get_result(test));
    cru_test_destroy(test);
}

static void
slave_loop(const cru_slave_t *slave)
{
    ASSERT_IN_SLAVE_PROCESS(slave);

    const cru_test_def_t *def;

    while (true) {
        def = slave_recv_dispatch(slave);
        if (!def)
            return;

        slave_run_test(slave, def);
    }
}

static void
master_drain_result_pipe(cru_slave_t *slave)
{
    ASSERT_IN_MASTER_PROCESS(slave);

    while (slave->num_active_tests > 0) {
        if (!master_recv_result(slave)) {
            break;
        }
    }
}

static bool
master_init_slave(cru_slave_t *slave)
{
    ASSERT_IN_MASTER_PROCESS(slave);
    assert(slave->pid == -1);

    slave->num_active_tests = 0;

    if (!cru_pipe_init(&slave->dispatch_pipe))
        return false;

    if (!cru_pipe_init(&slave->result_pipe))
        return false;

    slave->pid = fork();

    if (slave->pid == -1) {
        cru_loge("test runner failed to fork slave process");
        return false;
    }

    if (slave->pid == 0) {
        if (!cru_pipe_become_reader(&slave->dispatch_pipe))
            exit(EXIT_FAILURE);

        if (!cru_pipe_become_writer(&slave->result_pipe))
            exit(EXIT_FAILURE);

        slave_loop(slave);

        exit(EXIT_SUCCESS);
    }

    if (!cru_pipe_become_writer(&slave->dispatch_pipe))
        return false;

    if (!cru_pipe_become_reader(&slave->result_pipe))
        return false;

    return true;
}

static void
master_cleanup_slave(cru_slave_t *slave)
{
    ASSERT_IN_MASTER_PROCESS(slave);

    if (slave->pid == -1)
        return;

    master_drain_result_pipe(slave);

    slave->num_active_tests = 0;
    cru_pipe_finish(&slave->dispatch_pipe);
    cru_pipe_finish(&slave->result_pipe);

    if (waitpid(slave->pid, /*status*/ NULL, /*flags*/ 0) == -1) {
        cru_loge("runner failed to wait for slave process");
    }

    slave->pid = -1;
}

/// Dispatch tests to slave processes.
static void
master_loop(void)
{
    const cru_test_def_t *def;
    cru_slave_t *slave = &master.slaves[0];

    if (!master_init_slave(slave))
        return;

    // Dispatch each test to the current slave process. Interleave the
    // dispatching and result collection.
    cru_foreach_test_def(def) {
        ASSERT_IN_MASTER_PROCESS(slave);

        if (!def->priv.enable)
            continue;

        if (def->skip) {
            master_report_result(def, CRU_TEST_RESULT_SKIP);
            continue;
        }

        cru_log_tag("start", "%s", def->name);

        if (!master_send_dispatch(slave, def)) {
            // The slave is probably be dead. Reap the zombie and spawn a new
            // one.  Re-dispatch this test to the new slave.
            master_drain_result_pipe(slave);
            master_cleanup_slave(slave);

            if (!master_init_slave(slave)) {
                // Can't recover.
                return;
            }

            if (!master_send_dispatch(slave, def)) {
                // We twice failed to dispatch this test. Give up.
                return;
            }
        }

        master_drain_result_pipe(slave);
    }

    // Tell the slave that it will receive no more tests.
    master_send_dispatch(slave, NULL);
    master_cleanup_slave(slave);
}

/// Return true if and only all tests pass or skip.
bool
cru_runner_run_tests(void)
{
    cru_log_align_tags(true);
    cru_logi("running %u tests", master.num_tests);
    cru_logi("================================");

    master_loop();

    uint32_t num_ran = master.num_pass + master.num_fail + master.num_skip;
    uint32_t num_lost = master.num_tests - num_ran;

    // A big, and perhaps unneeded, hammer.
    fflush(stdout);
    fflush(stderr);

    cru_logi("================================");
    cru_logi("ran %u tests", master.num_tests);
    cru_logi("pass %u", master.num_pass);
    cru_logi("fail %u", master.num_fail);
    cru_logi("skip %u", master.num_skip);
    cru_logi("lost %u", num_lost);

    return master.num_pass + master.num_skip == master.num_tests;
}

static inline void
enable_test_def(cru_test_def_t *def)
{
    if (!def->priv.enable)
        ++master.num_tests;

    def->priv.enable = true;
}

void
cru_runner_enable_all_nonexample_tests(void)
{
    cru_test_def_t *def;

    cru_foreach_test_def(def) {
        if (!cru_test_def_match(def, "example.*")) {
            enable_test_def(def);
        }
    }
}

void
cru_runner_enable_matching_tests(const cru_cstr_vec_t *testname_globs)
{
    cru_test_def_t *def;
    char **glob;

    cru_foreach_test_def(def) {
        cru_vec_foreach(glob, testname_globs) {
            if (cru_test_def_match(def, *glob)) {
                enable_test_def(def);
            }
        }
    }
}
