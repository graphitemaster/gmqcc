#include <vector>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <dirent.h>
#include <unistd.h>

#include "gmqcc.h"

static const char *task_bins[] = {
    "./gmqcc",
    "./qcvm"
};

struct popen_t {
    FILE *handles[3];
    int pipes[3];
    int stderr_fd;
    int stdout_fd;
    int pid;
};

static FILE **task_popen(const char *command, const char *mode) {
    int     inhandle  [2];
    int     outhandle [2];
    int     errhandle [2];
    int     trypipe;

    popen_t *data = (popen_t*)mem_a(sizeof(popen_t));

    char *line = (char*)command;
    std::vector<char *> argv;
    {

        while (*line != '\0') {
            while (*line == ' ' || *line == '\t' || *line == '\n')
                *line++ = '\0';
            argv.push_back(line);
            while (*line != '\0' && *line != ' ' &&
                   *line != '\t' && *line != '\n') line++;
        }
        argv.push_back((char *)0);
    }

    if ((trypipe = pipe(inhandle))  < 0) goto task_popen_error_0;
    if ((trypipe = pipe(outhandle)) < 0) goto task_popen_error_1;
    if ((trypipe = pipe(errhandle)) < 0) goto task_popen_error_2;

    if ((data->pid = fork()) > 0) {
        /* parent */
        close(inhandle [0]);
        close(outhandle [1]);
        close(errhandle [1]);
        data->pipes[0] = inhandle [1];
        data->pipes[1] = outhandle[0];
        data->pipes[2] = errhandle[0];
        data->handles[0] = fdopen(inhandle [1], "w");
        data->handles[1] = fdopen(outhandle[0], mode);
        data->handles[2] = fdopen(errhandle[0], mode);
        return data->handles;
    } else if (data->pid == 0) {
        /* child */
        close(inhandle [1]);
        close(outhandle[0]);
        close(errhandle[0]);

        /* see piping documentation for this sillyness :P */
        dup2(inhandle [0], 0);
        dup2(outhandle[1], 1);
        dup2(errhandle[1], 2);

        execvp(argv[0], &argv[0]);
        exit(95);
    } else {
        /* fork failed */
        goto task_popen_error_3;
    }

task_popen_error_3: close(errhandle[0]), close(errhandle[1]);
task_popen_error_2: close(outhandle[0]), close(outhandle[1]);
task_popen_error_1: close(inhandle [0]), close(inhandle [1]);
task_popen_error_0:

    return nullptr;
}

static int task_pclose(FILE **handles) {
    popen_t *data   = (popen_t*)handles;
    int      status = 0;

    close(data->pipes[0]); /* stdin  */
    close(data->pipes[1]); /* stdout */
    close(data->pipes[2]); /* stderr */

    if (data->pid != waitpid(data->pid, &status, 0)) {
      abort();
    }
    if (!WIFEXITED(status))
      return -1;
    if (WIFSIGNALED(status))
      con_out("got signaled!\n");

    mem_d(data);

    return status ? 1 : 0;
}

#define TASK_COMPILE    0
#define TASK_EXECUTE    1
/*
 * Task template system:
 *  templates are rules for a specific test, used to create a "task" that
 *  is executed with those set of rules (arguments, and what not). Tests
 *  that don't have a template with them cannot become tasks, since without
 *  the information for that test there is no way to properly "test" them.
 *  Rules for these templates are described in a template file, using a
 *  task template language.
 *
 *  The language is a basic finite statemachine, top-down single-line
 *  description language.
 *
 *  The languge is composed entierly of "tags" which describe a string of
 *  text for a task.  Think of it much like a configuration file.  Except
 *  it's been designed to allow flexibility and future support for prodecual
 *  semantics.
 *
 *  The following "tags" are suported by the language
 *
 *      D:
 *          Used to set a description of the current test, this must be
 *          provided, this tag is NOT optional.
 *
 *      T:
 *          Used to set the procedure for the given task, there are two
 *          options for this:
 *              -compile
 *                  This simply performs compilation only
 *              -execute
 *                  This will perform compilation and execution
 *              -fail
 *                  This will perform compilation, but requires
 *                  the compilation to fail in order to succeed.
 *
 *          This must be provided, this tag is NOT optional.
 *
 *      C:
 *          Used to set the compilation flags for the given task, this
 *          must be provided, this tag is NOT optional.
 *
 *      F:  Used to set some test suite flags, currently the only option
 *          is -no-defs (to including of defs.qh)
 *
 *      E:
 *          Used to set the execution flags for the given task. This tag
 *          must be provided if T == -execute, otherwise it's erroneous
 *          as compilation only takes place.
 *
 *      M:
 *          Used to describe a string of text that should be matched from
 *          the output of executing the task.  If this doesn't match the
 *          task fails.  This tag must be provided if T == -execute, otherwise
 *          it's erroneous as compilation only takes place.
 *
 *      I:
 *          Used to specify the INPUT source file to operate on, this must be
 *          provided, this tag is NOT optional
 *
 *
 *  Notes:
 *      These tags have one-time use, using them more than once will result
 *      in template compilation errors.
 *
 *      Lines beginning with # or // in the template file are comments and
 *      are ignored by the template parser.
 *
 *      Whitespace is optional, with exception to the colon ':' between the
 *      tag and it's assignment value/
 *
 *      The template compiler will detect erronrous tags (optional tags
 *      that need not be set), as well as missing tags, and error accordingly
 *      this will result in the task failing.
 */
struct task_template_t {
    char *description;
    char *compileflags;
    char *executeflags;
    char *proceduretype;
    char *sourcefile;
    char *tempfilename;
    std::vector<char *> comparematch;
    char *rulesfile;
    char *testflags;
};

/*
 * This is very much like a compiler code generator :-).  This generates
 * a value from some data observed from the compiler.
 */
static bool task_template_generate(task_template_t *tmpl, char tag, const char *file, size_t line, char *value, size_t *pad) {
    size_t desclen = 0;
    size_t filelen = 0;
    char **destval = nullptr;

    if (!tmpl)
        return false;

    switch(tag) {
        case 'D': destval = &tmpl->description;    break;
        case 'T': destval = &tmpl->proceduretype;  break;
        case 'C': destval = &tmpl->compileflags;   break;
        case 'E': destval = &tmpl->executeflags;   break;
        case 'I': destval = &tmpl->sourcefile;     break;
        case 'F': destval = &tmpl->testflags;      break;
        default:
            con_printmsg(LVL_ERROR, __FILE__, __LINE__, 0, "internal error",
                "invalid tag `%c:` during code generation\n",
                tag
            );
            return false;
    }

    /*
     * Ensure if for the given tag, there already exists a
     * assigned value.
     */
    if (*destval) {
        con_printmsg(LVL_ERROR, file, line, 0, /*TODO: column for match*/ "compile error",
            "tag `%c:` already assigned value: %s\n",
            tag, *destval
        );
        return false;
    }

    /*
     * Strip any whitespace that might exist in the value for assignments
     * like "D:      foo"
     */
    if (value && *value && (*value == ' ' || *value == '\t'))
        value++;
    else if (!value)
        exit(EXIT_FAILURE);

    /*
     * Value will contain a newline character at the end, we need to strip
     * this otherwise kaboom, seriously, kaboom :P
     */
    if (strchr(value, '\n'))
        *strrchr(value, '\n')='\0';

    /*
     * Now allocate and set the actual value for the specific tag. Which
     * was properly selected and can be accessed with *destval.
     */
    *destval = util_strdup(value);


    if (*destval == tmpl->description) {
        /*
         * Create some padding for the description to align the
         * printing of the rules file.
         */
        if ((desclen = strlen(tmpl->description)) > pad[0])
            pad[0] = desclen;
    }

    if ((filelen = strlen(file)) > pad[2])
        pad[2] = filelen;

    return true;
}

static bool task_template_parse(const char *file, task_template_t *tmpl, FILE *fp, size_t *pad) {
    char  *data = nullptr;
    char  *back = nullptr;
    size_t size = 0;
    size_t line = 1;

    if (!tmpl)
        return false;

    /* top down parsing */
    while (util_getline(&back, &size, fp) != EOF) {
        /* skip whitespace */
        data = back;
        if (*data && (*data == ' ' || *data == '\t'))
            data++;

        switch (*data) {
            /*
             * Handle comments inside task tmpl files.  We're strict
             * about the language for fun :-)
             */
            case '/':
                if (data[1] != '/') {
                    con_printmsg(LVL_ERROR, file, line, 0, /*TODO: column for match*/ "tmpl parse error",
                        "invalid character `/`, perhaps you meant `//` ?");

                    mem_d(back);
                    return false;
                }
            case '#':
                break;

            /*
             * Empty newlines are acceptable as well, so we handle that here
             * despite being just odd since there should't be that many
             * empty lines to begin with.
             */
            case '\r':
            case '\n':
                break;


            /*
             * Now begin the actual "tag" stuff.  This works as you expect
             * it to.
             */
            case 'D':
            case 'T':
            case 'C':
            case 'E':
            case 'I':
            case 'F':
                if (data[1] != ':') {
                    con_printmsg(LVL_ERROR, file, line, 0, /*TODO: column for match*/ "tmpl parse error",
                        "expected `:` after `%c`",
                        *data
                    );
                    goto failure;
                }
                if (!task_template_generate(tmpl, *data, file, line, &data[3], pad)) {
                    con_printmsg(LVL_ERROR, file, line, 0, /*TODO: column for match*/ "tmpl compile error",
                        "failed to generate for given task\n"
                    );
                    goto failure;
                }
                break;

            /*
             * Match requires it's own system since we allow multiple M's
             * for multi-line matching.
             */
            case 'M':
            {
                char *value = &data[3];
                if (data[1] != ':') {
                    con_printmsg(LVL_ERROR, file, line, 0, /*TODO: column for match*/ "tmpl parse error",
                        "expected `:` after `%c`",
                        *data
                    );
                    goto failure;
                }

                /*
                 * Value will contain a newline character at the end, we need to strip
                 * this otherwise kaboom, seriously, kaboom :P
                 */
                if (strrchr(value, '\n'))
                    *strrchr(value, '\n')='\0';
                else /* cppcheck: possible null pointer dereference */
                    exit(EXIT_FAILURE);

                tmpl->comparematch.push_back(util_strdup(value));

                break;
            }

            default:
                con_printmsg(LVL_ERROR, file, line, 0, /*TODO: column for match*/ "tmpl parse error",
                    "invalid tag `%c`", *data
                );
                goto failure;
            /* no break required */
        }

        /* update line and free old sata */
        line++;
        mem_d(back);
        back = nullptr;
    }
    if (back)
        mem_d(back);
    return true;

failure:
    mem_d (back);
    return false;
}

/*
 * Nullifies the template data: used during initialization of a new
 * template and free.
 */
static void task_template_nullify(task_template_t *tmpl) {
    if (!tmpl)
        return;

    tmpl->description = nullptr;
    tmpl->proceduretype = nullptr;
    tmpl->compileflags = nullptr;
    tmpl->executeflags = nullptr;
    tmpl->sourcefile = nullptr;
    tmpl->tempfilename = nullptr;
    tmpl->rulesfile = nullptr;
    tmpl->testflags = nullptr;
}

static task_template_t *task_template_compile(const char *file, const char *dir, size_t *pad) {
    /* a page should be enough */
    char             fullfile[4096];
    size_t           filepadd = 0;
    FILE       *tempfile = nullptr;
    task_template_t *tmpl     = nullptr;

    util_snprintf(fullfile, sizeof(fullfile), "%s/%s", dir, file);

    tempfile = fopen(fullfile, "r");
    tmpl = (task_template_t*)mem_a(sizeof(task_template_t));
    new (tmpl) task_template_t();
    task_template_nullify(tmpl);

    /*
     * Create some padding for the printing to align the
     * printing of the rules file to the console.
     */
    if ((filepadd = strlen(fullfile)) > pad[1])
        pad[1] = filepadd;

    tmpl->rulesfile = util_strdup(fullfile);

    /*
     * Esnure the file even exists for the task, this is pretty useless
     * to even do.
     */
    if (!tempfile) {
        con_err("template file: %s does not exist or invalid permissions\n",
            file
        );
        goto failure;
    }

    if (!task_template_parse(file, tmpl, tempfile, pad)) {
        con_err("template parse error: error during parsing\n");
        goto failure;
    }

    /*
     * Regardless procedure type, the following tags must exist:
     *  D
     *  T
     *  C
     *  I
     */
    if (!tmpl->description) {
        con_err("template compile error: %s missing `D:` tag\n", file);
        goto failure;
    }
    if (!tmpl->proceduretype) {
        con_err("template compile error: %s missing `T:` tag\n", file);
        goto failure;
    }
    if (!tmpl->compileflags) {
        con_err("template compile error: %s missing `C:` tag\n", file);
        goto failure;
    }
    if (!tmpl->sourcefile) {
        con_err("template compile error: %s missing `I:` tag\n", file);
        goto failure;
    }

    /*
     * Now lets compile the template, compilation is really just
     * the process of validating the input.
     */
    if (!strcmp(tmpl->proceduretype, "-compile")) {
        if (tmpl->executeflags)
            con_err("template compile warning: %s erroneous tag `E:` when only compiling\n", file);
        if (tmpl->comparematch.size())
            con_err("template compile warning: %s erroneous tag `M:` when only compiling\n", file);
        goto success;
    } else if (!strcmp(tmpl->proceduretype, "-execute")) {
        if (!tmpl->executeflags) {
            /* default to $null */
            tmpl->executeflags = util_strdup("$null");
        }
        if (tmpl->comparematch.empty()) {
            con_err("template compile error: %s missing `M:` tag (use `$null` for exclude)\n", file);
            goto failure;
        }
    } else if (!strcmp(tmpl->proceduretype, "-fail")) {
        if (tmpl->executeflags)
            con_err("template compile warning: %s erroneous tag `E:` when only failing\n", file);
        if (tmpl->comparematch.size())
            con_err("template compile warning: %s erroneous tag `M:` when only failing\n", file);
    } else if (!strcmp(tmpl->proceduretype, "-diagnostic")) {
        if (tmpl->executeflags)
            con_err("template compile warning: %s erroneous tag `E:` when only diagnostic\n", file);
        if (tmpl->comparematch.empty()) {
            con_err("template compile error: %s missing `M:` tag (use `$null` for exclude)\n", file);
            goto failure;
        }
    } else if (!strcmp(tmpl->proceduretype, "-pp")) {
        if (tmpl->executeflags)
            con_err("template compile warning: %s erroneous tag `E:` when only preprocessing\n", file);
        if (tmpl->comparematch.empty()) {
            con_err("template compile error: %s missing `M:` tag (use `$null` for exclude)\n", file);
            goto failure;
        }
    } else {
        con_err("template compile error: %s invalid procedure type: %s\n", file, tmpl->proceduretype);
        goto failure;
    }

success:
    fclose(tempfile);
    return tmpl;

failure:
    /*
     * The file might not exist and we jump here when that doesn't happen
     * so the check to see if it's not null here is required.
     */
    if (tempfile)
        fclose(tempfile);
    mem_d(tmpl);

    return nullptr;
}

static void task_template_destroy(task_template_t *tmpl) {
    if (!tmpl)
        return;

    if (tmpl->description)    mem_d(tmpl->description);
    if (tmpl->proceduretype)  mem_d(tmpl->proceduretype);
    if (tmpl->compileflags)   mem_d(tmpl->compileflags);
    if (tmpl->executeflags)   mem_d(tmpl->executeflags);
    if (tmpl->sourcefile)     mem_d(tmpl->sourcefile);
    if (tmpl->rulesfile)      mem_d(tmpl->rulesfile);
    if (tmpl->testflags)      mem_d(tmpl->testflags);


    for (auto &it : tmpl->comparematch)
        mem_d(it);

    /*
     * Nullify all the template members otherwise nullptr comparision
     * checks will fail if tmpl pointer is reused.
     */
    mem_d(tmpl->tempfilename);
    mem_d(tmpl);
}

/*
 * Now comes the task manager, this system allows adding tasks in and out
 * of a task list.  This is the executor of the tasks essentially as well.
 */
struct task_t {
    task_template_t *tmpl;
    FILE **runhandles;
    FILE *stderrlog;
    FILE *stdoutlog;
    char *stdoutlogfile;
    char *stderrlogfile;
    bool compiled;
};

static std::vector<task_t> task_tasks;

/*
 * Read a directory and searches for all template files in it
 * which is later used to run all tests.
 */
static bool task_propagate(const char *curdir, size_t *pad, const char *defs) {
    bool  success = true;
    DIR *dir;
    struct dirent *files;
    struct stat directory;
    char buffer[4096];
    size_t found = 0;
    std::vector<char *> directories;
    char *claim = util_strdup(curdir);

    directories.push_back(claim);
    dir = opendir(claim);

    /*
     * Generate a list of subdirectories since we'll be checking them too
     * for tmpl files.
     */
    while ((files = readdir(dir))) {
        util_asprintf(&claim, "%s/%s", curdir, files->d_name);
        if (stat(claim, &directory) == -1) {
            closedir(dir);
            mem_d(claim);
            return false;
        }

        if (S_ISDIR(directory.st_mode) && files->d_name[0] != '.') {
            directories.push_back(claim);
        } else {
            mem_d(claim);
            claim = nullptr;
        }
    }
    closedir(dir);

    /*
     * Now do all the work, by touching all the directories inside
     * test as well and compile the task templates into data we can
     * use to run the tests.
     */
    for (auto &it : directories) {
        dir = opendir(it);
        while ((files = readdir(dir))) {
            util_snprintf(buffer, sizeof(buffer), "%s/%s", it, files->d_name);
            if (stat(buffer, &directory) == -1) {
                con_err("internal error: stat failed, aborting\n");
                abort();
            }

            if (S_ISDIR(directory.st_mode))
                continue;

            /*
             * We made it here, which concludes the file/directory is not
             * actually a directory, so it must be a file :)
             */
            if (strcmp(files->d_name + strlen(files->d_name) - 5, ".tmpl") == 0) {
                task_template_t *tmpl = task_template_compile(files->d_name, it, pad);
                char             buf[4096]; /* one page should be enough */
                const char      *qcflags = nullptr;
                task_t           task;

                found ++;
                if (!tmpl) {
                    con_err("error compiling task template: %s\n", files->d_name);
                    success = false;
                    continue;
                }
                /*
                 * Generate a temportary file name for the output binary
                 * so we don't trample over an existing one.
                 */
                tmpl->tempfilename = nullptr;
                util_asprintf(&tmpl->tempfilename, "%s/TMPDAT.%s.dat", it, files->d_name);

                /*
                 * Additional QCFLAGS enviroment variable may be used
                 * to test compile flags for all tests.  This needs to be
                 * BEFORE other flags (so that the .tmpl can override them)
                 */
                qcflags = getenv("QCFLAGS");

                /*
                 * Generate the command required to open a pipe to a process
                 * which will be refered to with a handle in the task for
                 * reading the data from the pipe.
                 */
                if (strcmp(tmpl->proceduretype, "-pp")) {
                    if (qcflags) {
                        if (tmpl->testflags && !strcmp(tmpl->testflags, "-no-defs")) {
                            util_snprintf(buf, sizeof(buf), "%s %s/%s %s %s -o %s",
                                task_bins[TASK_COMPILE],
                                it,
                                tmpl->sourcefile,
                                qcflags,
                                tmpl->compileflags,
                                tmpl->tempfilename
                            );
                        } else {
                            util_snprintf(buf, sizeof(buf), "%s %s/%s %s/%s %s %s -o %s",
                                task_bins[TASK_COMPILE],
                                curdir,
                                defs,
                                it,
                                tmpl->sourcefile,
                                qcflags,
                                tmpl->compileflags,
                                tmpl->tempfilename
                            );
                        }
                    } else {
                        if (tmpl->testflags && !strcmp(tmpl->testflags, "-no-defs")) {
                            util_snprintf(buf, sizeof(buf), "%s %s/%s %s -o %s",
                                task_bins[TASK_COMPILE],
                                it,
                                tmpl->sourcefile,
                                tmpl->compileflags,
                                tmpl->tempfilename
                            );
                        } else {
                            util_snprintf(buf, sizeof(buf), "%s %s/%s %s/%s %s -o %s",
                                task_bins[TASK_COMPILE],
                                curdir,
                                defs,
                                it,
                                tmpl->sourcefile,
                                tmpl->compileflags,
                                tmpl->tempfilename
                            );
                        }
                    }
                } else {
                    /* Preprocessing (qcflags mean shit all here we don't allow them) */
                    if (tmpl->testflags && !strcmp(tmpl->testflags, "-no-defs")) {
                        util_snprintf(buf, sizeof(buf), "%s -E %s/%s %s -o %s",
                            task_bins[TASK_COMPILE],
                            it,
                            tmpl->sourcefile,
                            tmpl->compileflags,
                            tmpl->tempfilename
                        );
                    } else {
                        util_snprintf(buf, sizeof(buf), "%s -E %s/%s %s/%s %s -o %s",
                            task_bins[TASK_COMPILE],
                            curdir,
                            defs,
                            it,
                            tmpl->sourcefile,
                            tmpl->compileflags,
                            tmpl->tempfilename
                        );
                    }
                }

                /*
                 * The task template was compiled, now lets create a task from
                 * the template data which has now been propagated.
                 */
                task.tmpl = tmpl;
                if (!(task.runhandles = task_popen(buf, "r"))) {
                    con_err("error opening pipe to process for test: %s\n", tmpl->description);
                    success = false;
                    continue;
                }

                /*
                 * Open up some file desciptors for logging the stdout/stderr
                 * to our own.
                 */
                util_snprintf(buf, sizeof(buf), "%s.stdout", tmpl->tempfilename);
                task.stdoutlogfile = util_strdup(buf);
                if (!(task.stdoutlog = fopen(buf, "w"))) {
                    con_err("error opening %s for stdout\n", buf);
                    continue;
                }

                util_snprintf(buf, sizeof(buf), "%s.stderr", tmpl->tempfilename);
                task.stderrlogfile = util_strdup(buf);
                if (!(task.stderrlog = fopen(buf, "w"))) {
                    con_err("error opening %s for stderr\n", buf);
                    continue;
                }
                task_tasks.push_back(task);
            }
        }
        closedir(dir);
        mem_d(it); /* free claimed memory */
    }
    return success;
}

/*
 * Task precleanup removes any existing temporary files or log files
 * left behind from a previous invoke of the test-suite.
 */
static void task_precleanup(const char *curdir) {
    DIR     *dir;
    struct dirent  *files;
    char          buffer[4096];

    dir = opendir(curdir);

    while ((files = readdir(dir))) {
        if (strstr(files->d_name, "TMP")     ||
            strstr(files->d_name, ".stdout") ||
            strstr(files->d_name, ".stderr") ||
            strstr(files->d_name, ".dat"))
        {
            util_snprintf(buffer, sizeof(buffer), "%s/%s", curdir, files->d_name);
            if (remove(buffer))
                con_err("error removing temporary file: %s\n", buffer);
        }
    }

    closedir(dir);
}

static void task_destroy(void) {
    /*
     * Free all the data in the task list and finally the list itself
     * then proceed to cleanup anything else outside the program like
     * temporary files.
     */
    for (auto &it : task_tasks) {
        /*
         * Close any open handles to files or processes here.  It's mighty
         * annoying to have to do all this cleanup work.
         */
        if (it.stdoutlog) fclose(it.stdoutlog);
        if (it.stderrlog) fclose(it.stderrlog);

        /*
         * Only remove the log files if the test actually compiled otherwise
         * forget about it (or if it didn't compile, and the procedure type
         * was set to -fail (meaning it shouldn't compile) .. stil remove)
         */
        if (it.compiled || !strcmp(it.tmpl->proceduretype, "-fail")) {
            if (remove(it.stdoutlogfile))
                con_err("error removing stdout log file: %s\n", it.stdoutlogfile);
            if (remove(it.stderrlogfile))
                con_err("error removing stderr log file: %s\n", it.stderrlogfile);

            (void)!remove(it.tmpl->tempfilename);
        }

        /* free util_strdup data for log files */
        mem_d(it.stdoutlogfile);
        mem_d(it.stderrlogfile);

        task_template_destroy(it.tmpl);
    }
}

/*
 * This executes the QCVM task for a specificly compiled progs.dat
 * using the template passed into it for call-flags and user defined
 * messages IF the procedure type is -execute, otherwise it matches
 * the preprocessor output.
 */
static bool task_trymatch(task_t &task, std::vector<char *> &line) {
    bool success = true;
    bool process = true;
    int retval = EXIT_SUCCESS;
    FILE *execute;
    char buffer[4096];
    task_template_t *tmpl = task.tmpl;

    memset(buffer,0,sizeof(buffer));

    if (!strcmp(tmpl->proceduretype, "-execute")) {
        /*
         * Drop the execution flags for the QCVM if none where
         * actually specified.
         */
        if (!strcmp(tmpl->executeflags, "$null")) {
            util_snprintf(buffer,  sizeof(buffer), "%s %s",
                task_bins[TASK_EXECUTE],
                tmpl->tempfilename
            );
        } else {
            util_snprintf(buffer,  sizeof(buffer), "%s %s %s",
                task_bins[TASK_EXECUTE],
                tmpl->executeflags,
                tmpl->tempfilename
            );
        }

        execute = popen(buffer, "r");
        if (!execute)
            return false;
    } else if (!strcmp(tmpl->proceduretype, "-pp")) {
        /*
         * we're preprocessing, which means we need to read int
         * the produced file and do some really weird shit.
         */
        if (!(execute = fopen(tmpl->tempfilename, "r")))
            return false;
        process = false;
    } else {
        /*
         * we're testing diagnostic output, which means it will be
         * in runhandles[2] (stderr) since that is where the compiler
         * puts it's errors.
         */
        if (!(execute = fopen(task.stderrlogfile, "r")))
            return false;
        process = false;
    }

    /*
     * Now lets read the lines and compare them to the matches we expect
     * and handle accordingly.
     */
    {
        char  *data    = nullptr;
        size_t size    = 0;
        size_t compare = 0;

        while (util_getline(&data, &size, execute) != EOF) {
            if (!strcmp(data, "No main function found\n")) {
                con_err("test failure: `%s` (No main function found) [%s]\n",
                    tmpl->description,
                    tmpl->rulesfile
                );
                if (!process)
                    fclose(execute);
                else
                    pclose((FILE*)execute);
                return false;
            }

            /*
             * Trim newlines from data since they will just break our
             * ability to properly validate matches.
             */
            if  (strrchr(data, '\n'))
                *strrchr(data, '\n') = '\0';

            /*
             * We remove the file/directory and stuff from the error
             * match messages when testing diagnostics.
             */
            if(!strcmp(tmpl->proceduretype, "-diagnostic")) {
                if (strstr(data, "there have been errors, bailing out"))
                    continue; /* ignore it */
                if (strstr(data, ": error: ")) {
                    char *claim = util_strdup(data + (strstr(data, ": error: ") - data) + 9);
                    mem_d(data);
                    data = claim;
                }
            }

            /*
             * We need to ignore null lines for when -pp is used (preprocessor), since
             * the preprocessor is likely to create empty newlines in certain macro
             * instantations, otherwise it's in the wrong nature to ignore empty newlines.
             */
            if (!strcmp(tmpl->proceduretype, "-pp") && !*data)
                continue;

            if (tmpl->comparematch.size() > compare) {
                if (strcmp(data, tmpl->comparematch[compare++])) {
                    success = false;
                }
            } else {
                success = false;
            }

            line.push_back(data);

            /* reset */
            data = nullptr;
            size = 0;
        }

        if (compare != tmpl->comparematch.size())
            success = false;

        mem_d(data);
        data = nullptr;
    }

    if (process)
        retval = pclose((FILE*)execute);
    else
        fclose(execute);

    return success && retval == EXIT_SUCCESS;
}

static const char *task_type(task_template_t *tmpl) {
    if (!strcmp(tmpl->proceduretype, "-pp"))
        return "type: preprocessor";
    if (!strcmp(tmpl->proceduretype, "-execute"))
        return "type: execution";
    if (!strcmp(tmpl->proceduretype, "-compile"))
        return "type: compile";
    if (!strcmp(tmpl->proceduretype, "-diagnostic"))
        return "type: diagnostic";
    return "type: fail";
}

/*
 * This schedualizes all tasks and actually runs them individually
 * this is generally easy for just -compile variants.  For compile and
 * execution this takes more work since a task needs to be generated
 * from thin air and executed INLINE.
 */
#include <math.h>
static size_t task_schedualize(size_t *pad) {
    char space[2][64];
    bool execute = false;
    char *data = nullptr;
    std::vector<char *> match;
    size_t size = 0;
    size_t i = 0;
    size_t failed = 0;
    int status = 0;

    util_snprintf(space[0], sizeof(space[0]), "%d", (int)task_tasks.size());

    for (auto &it : task_tasks) {
        i++;
        memset(space[1], 0, sizeof(space[1]));
        util_snprintf(space[1], sizeof(space[1]), "%d", (int)(i));

        con_out("test #%u %*s", i, strlen(space[0]) - strlen(space[1]), "");
            //con_out("[[%*s]]",
            //    (pad[0] + pad[1] - strlen(it.tmpl->description)) + (strlen(it.tmpl->rulesfile) - pad[1]),
            //    it.tmpl->rulesfile);
            //fflush(stdout);

        /*
         * Generate a task from thin air if it requires execution in
         * the QCVM.
         */

        /* diagnostic is not executed, but compare tested instead, like preproessor */
        execute = !! (!strcmp(it.tmpl->proceduretype, "-execute")) ||
                     (!strcmp(it.tmpl->proceduretype, "-pp"))      ||
                     (!strcmp(it.tmpl->proceduretype, "-diagnostic"));

        /*
         * We assume it compiled before we actually compiled :).  On error
         * we change the value
         */
        it.compiled = true;

        /*
         * Read data from stdout first and pipe that stuff into a log file
         * then we do the same for stderr.
         */
        while (util_getline(&data, &size, it.runhandles[1]) != EOF) {
            fputs(data, it.stdoutlog);

            if (strstr(data, "failed to open file")) {
                it.compiled = false;
                execute                = false;
            }
        }
        while (util_getline(&data, &size, it.runhandles[2]) != EOF) {
            /*
             * If a string contains an error we just dissalow execution
             * of it in the vm.
             *
             * TODO: make this more percise, e.g if we print a warning
             * that refers to a variable named error, or something like
             * that .. then this will blowup :P
             */
            if (strstr(data, "error") && strcmp(it.tmpl->proceduretype, "-diagnostic")) {
                execute                = false;
                it.compiled = false;
            }

            fputs(data, it.stderrlog);
            fflush(it.stderrlog); /* fast flush for read */
        }

        if (!it.compiled && strcmp(it.tmpl->proceduretype, "-fail")) {
            con_out("failure:   `%s` %*s %*s\n",
                it.tmpl->description,
                (pad[0] + pad[1] - strlen(it.tmpl->description)) + (strlen(it.tmpl->rulesfile) - pad[1]),
                it.tmpl->rulesfile,
                (pad[1] + pad[2] - strlen(it.tmpl->rulesfile)) + (strlen("(failed to compile)") - pad[2]),
                "(failed to compile)"
            );
            failed++;
            continue;
        }

        status = task_pclose(it.runhandles);
        if (status != 0 && status != 1) {
            con_out("compiler failure (returned: %i):   `%s` %*s\n",
                status,
                it.tmpl->description,
                (pad[0] + pad[1] - strlen(it.tmpl->description)) + (strlen(it.tmpl->rulesfile) - pad[1]),
                it.tmpl->rulesfile
            );
            failed++;
            continue;
        }
        if ((!strcmp(it.tmpl->proceduretype, "-fail") && status == EXIT_SUCCESS)
        ||  ( strcmp(it.tmpl->proceduretype, "-fail") && status == EXIT_FAILURE)) {
            con_out("failure:   `%s` %*s %*s\n",
                it.tmpl->description,
                (pad[0] + pad[1] - strlen(it.tmpl->description)) + (strlen(it.tmpl->rulesfile) - pad[1]),
                it.tmpl->rulesfile,
                (pad[1] + pad[2] - strlen(it.tmpl->rulesfile)) + (strlen("(compiler didn't return exit success)") - pad[2]),
                "(compiler didn't return exit success)"
            );
            failed++;
            continue;
        }

        if (!execute) {
            con_out("succeeded: `%s` %*s %*s\n",
                it.tmpl->description,
                (pad[0] + pad[1] - strlen(it.tmpl->description)) + (strlen(it.tmpl->rulesfile) - pad[1]),
                it.tmpl->rulesfile,
                (pad[1] + pad[2] - strlen(it.tmpl->rulesfile)) + (strlen(task_type(it.tmpl)) - pad[2]),
                task_type(it.tmpl)

            );
            continue;
        }

        /*
         * If we made it here that concludes the task is to be executed
         * in the virtual machine (or the preprocessor output needs to
         * be matched).
         */
        if (!task_trymatch(it, match)) {
            size_t d = 0;

            con_out("failure:   `%s` %*s %*s\n",
                it.tmpl->description,
                (pad[0] + pad[1] - strlen(it.tmpl->description)) + (strlen(it.tmpl->rulesfile) - pad[1]),
                it.tmpl->rulesfile,
                (pad[1] + pad[2] - strlen(it.tmpl->rulesfile)) + (strlen(
                    (strcmp(it.tmpl->proceduretype, "-pp"))
                        ? "(invalid results from execution)"
                        : (strcmp(it.tmpl->proceduretype, "-diagnostic"))
                            ? "(invalid results from preprocessing)"
                            : "(invalid results from compiler diagnsotics)"
                ) - pad[2]),
                (strcmp(it.tmpl->proceduretype, "-pp"))
                    ? "(invalid results from execution)"
                    : (strcmp(it.tmpl->proceduretype, "-diagnostic"))
                            ? "(invalid results from preprocessing)"
                            : "(invalid results from compiler diagnsotics)"
            );

            /*
             * Print nicely formatted expected match lists to console error
             * handler for the all the given matches in the template file and
             * what was actually returned from executing.
             */
            con_out("    Expected From %u Matches: (got %u Matches)\n",
                it.tmpl->comparematch.size(),
                match.size()
            );
            for (; d < it.tmpl->comparematch.size(); d++) {
                char *select = it.tmpl->comparematch[d];
                size_t length = 60 - strlen(select);
                con_out("        Expected: \"%s\"", select);
                while (length --)
                    con_out(" ");
                con_out("| Got: \"%s\"\n", (d >= match.size()) ? "<<nothing else to compare>>" : match[d]);
            }

            /*
             * Print the non-expected out (since we are simply not expecting it)
             * This will help track down bugs in template files that fail to match
             * something.
             */
            if (match.size() > it.tmpl->comparematch.size()) {
                for (d = 0; d < match.size() - it.tmpl->comparematch.size(); d++) {
                    con_out("        Expected: Nothing                                                       | Got: \"%s\"\n",
                        match[d + it.tmpl->comparematch.size()]
                    );
                }
            }

            for (auto &it : match)
                mem_d(it);
            match.clear();
            failed++;
            continue;
        }

        for (auto &it : match)
            mem_d(it);
        match.clear();

        con_out("succeeded: `%s` %*s %*s\n",
            it.tmpl->description,
            (pad[0] + pad[1] - strlen(it.tmpl->description)) + (strlen(it.tmpl->rulesfile) - pad[1]),
            it.tmpl->rulesfile,
            (pad[1] + pad[2] - strlen(it.tmpl->rulesfile)) + (strlen(task_type(it.tmpl))- pad[2]),
            task_type(it.tmpl)

        );
    }
    mem_d(data);
    return failed;
}

/*
 * This is the heart of the whole test-suite process.  This cleans up
 * any existing temporary files left behind as well as log files left
 * behind.  Then it propagates a list of tests from `curdir` by scaning
 * it for template files and compiling them into tasks, in which it
 * schedualizes them (executes them) and actually reports errors and
 * what not.  It then proceeds to destroy the tasks and return memory
 * it's the engine :)
 *
 * It returns true of tests could be propagated, otherwise it returns
 * false.
 *
 * It expects con_init() was called before hand.
 */
static GMQCC_WARN bool test_perform(const char *curdir, const char *defs) {
    size_t             failed       = false;
    static const char *default_defs = "defs.qh";

    size_t pad[] = {
        /* test ### [succeed/fail]: `description`      [tests/template.tmpl]     [type] */
                    0,                                 0,                        0
    };

    /*
     * If the default definition file isn't set to anything.  We will
     * use the default_defs here, which is "defs.qc"
     */
    if (!defs) {
        defs = default_defs;
    }


    task_precleanup(curdir);
    if (!task_propagate(curdir, pad, defs)) {
        con_err("error: failed to propagate tasks\n");
        task_destroy();
        return false;
    }
    /*
     * If we made it here all tasks where propagated from their resultant
     * template file.  So we can start the FILO scheduler, this has been
     * designed in the most thread-safe way possible for future threading
     * it's designed to prevent lock contention, and possible syncronization
     * issues.
     */
    failed = task_schedualize(pad);
    if (failed)
        con_out("%u out of %u tests failed\n", failed, task_tasks.size());
    task_destroy();

    return (failed) ? false : true;
}

/*
 * Fancy GCC-like LONG parsing allows things like --opt=param with
 * assignment operator.  This is used for redirecting stdout/stderr
 * console to specific files of your choice.
 */
static bool parsecmd(const char *optname, int *argc_, char ***argv_, char **out, int ds, bool split) {
    int  argc   = *argc_;
    char **argv = *argv_;

    size_t len = strlen(optname);

    if (strncmp(argv[0]+ds, optname, len))
        return false;

    /* it's --optname, check how the parameter is supplied */
    if (argv[0][ds+len] == '=') {
        *out = argv[0]+ds+len+1;
        return true;
    }

    if (!split || argc < ds) /* no parameter was provided, or only single-arg form accepted */
        return false;

    /* using --opt param */
    *out = argv[1];
    --*argc_;
    ++*argv_;
    return true;
}

int main(int argc, char **argv) {
    bool succeed  = false;
    char *defs = nullptr;

    con_init();

    /*
     * Command line option parsing commences now We only need to support
     * a few things in the test suite.
     */
    while (argc > 1) {
        ++argv;
        --argc;

        if (argv[0][0] == '-') {
            if (parsecmd("defs", &argc, &argv, &defs, 1, false))
                continue;

            if (!strcmp(argv[0]+1, "debug")) {
                OPTS_OPTION_BOOL(OPTION_DEBUG) = true;
                continue;
            }
            if (!strcmp(argv[0]+1, "nocolor")) {
                con_color(0);
                continue;
            }

            con_err("invalid argument %s\n", argv[0]+1);
            return -1;
        }
    }
    succeed = test_perform("tests", defs);

    return (succeed) ? EXIT_SUCCESS : EXIT_FAILURE;
}
