/*
 * Copyright (C) 2012
 *     Dale Weiler
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "gmqcc.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

bool  opts_memchk = false;
bool  opts_debug  = false;
char *task_bins[] = {
    "./gmqcc",
    "./qcvm"
};

/*
 * TODO: Windows version
 * this implements a unique bi-directional popen-like function that
 * allows reading data from both stdout and stderr. And writing to
 * stdin :)
 * 
 * Example of use:
 * FILE *handles[3] = task_popen("ls", "-l", "r");
 * if (!handles) { perror("failed to open stdin/stdout/stderr to ls");
 * // handles[0] = stdin
 * // handles[1] = stdout
 * // handles[2] = stderr
 * 
 * task_pclose(handles); // to close
 */
#ifndef _WIN32
#include <sys/types.h>
#include <sys/wait.h>

#include <unistd.h>
typedef struct {
    FILE *handles[3];
    int   pipes  [3];
    
    int stderr_fd;
    int stdout_fd;
    int pid;
} popen_t;

FILE ** task_popen(const char *command, const char *mode) {
    int     inhandle  [2];
    int     outhandle [2];
    int     errhandle [2];
    int     trypipe;
    
    popen_t *data = mem_a(sizeof(popen_t));
    
    /*
     * Parse the command now into a list for execv, this is a pain
     * in the ass.
     */
    char  *line = (char*)command;
    char **argv = NULL;
    {
        
        while (*line != '\0') {
            while (*line == ' ' || *line == '\t' || *line == '\n')
                *line++ = '\0';
            vec_push(argv, line);
            
            while (*line != '\0' && *line != ' ' &&
                   *line != '\t' && *line != '\n') line++;
        }
        vec_push(argv, '\0');
    }
    
    
    if ((trypipe = pipe(inhandle))  < 0) goto task_popen_error_0;
    if ((trypipe = pipe(outhandle)) < 0) goto task_popen_error_1;
    if ((trypipe = pipe(errhandle)) < 0) goto task_popen_error_2;
    
    if ((data->pid = fork()) > 0) {
        /* parent */
        close(inhandle  [0]);
        close(outhandle [1]);
        close(errhandle [1]);
        
        data->pipes  [0] = inhandle [1];
        data->pipes  [1] = outhandle[0];
        data->pipes  [2] = errhandle[0];
        data->handles[0] = fdopen(inhandle [1], "w");
        data->handles[1] = fdopen(outhandle[0], mode);
        data->handles[2] = fdopen(errhandle[0], mode);
        
        /* sigh */
        if (argv)
            vec_free(argv);
        return data->handles;
    } else if (data->pid == 0) {
        /* child */
        close(inhandle [1]);
        close(outhandle[0]);
        close(errhandle[0]);
        
        /* see piping documentation for this sillyness :P */
        close(0), dup(inhandle [0]);
        close(1), dup(outhandle[1]);
        close(2), dup(errhandle[1]);
        
        execvp(*argv, argv);
        exit(1);
    } else {
        /* fork failed */
        goto task_popen_error_3;
    }
    
    if (argv)
        vec_free(argv);
    return data->handles;
        
task_popen_error_3: close(errhandle[0]), close(errhandle[1]);
task_popen_error_2: close(outhandle[0]), close(outhandle[1]);
task_popen_error_1: close(inhandle [0]), close(inhandle [1]);
task_popen_error_0:

    if (argv)
        vec_free(argv);
    return NULL;
}

int task_pclose(FILE **handles) {
    popen_t *data   = (popen_t*)handles;
    int      status = 0;
    
    close(data->pipes[0]); /* stdin  */
    close(data->pipes[1]); /* stdout */
    close(data->pipes[2]); /* stderr */
    
    waitpid(data->pid, &status, 0);
    
    mem_d(data);
    
    return status;
}
#else

#endif

#define TASK_COMPILE 0
#define TASK_EXECUTE 1

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
 *      F:
 *          Used to set a failure message, this message will be displayed
 *          if the test fails, this tag is optional
 * 
 *      S:
 *          Used to set a success message, this message will be displayed
 *          if the test succeeds, this tag is optional.
 * 
 *      T:
 *          Used to set the procedure for the given task, there are two
 *          options for this:
 *              -compile
 *                  This simply performs compilation only
 *              -execute
 *                  This will perform compilation and execution
 * 
 *          This must be provided, this tag is NOT optional.
 * 
 *      C:
 *          Used to set the compilation flags for the given task, this
 *          must be provided, this tag is NOT optional.
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
typedef struct {
    char  *description;
    char  *failuremessage;
    char  *successmessage;
    char  *compileflags;
    char  *executeflags;
    char  *proceduretype;
    char  *sourcefile;
    char  *tempfilename;
    char **comparematch;
} task_template_t;

/*
 * This is very much like a compiler code generator :-).  This generates
 * a value from some data observed from the compiler.
 */
bool task_template_generate(task_template_t *template, char tag, const char *file, size_t line, const char *value) {
    char **destval = NULL;
    
    if (!template)
        return false;
    
    switch(tag) {
        case 'D': destval = &template->description;    break;
        case 'F': destval = &template->failuremessage; break;
        case 'S': destval = &template->successmessage; break;
        case 'T': destval = &template->proceduretype;  break;
        case 'C': destval = &template->compileflags;   break;
        case 'E': destval = &template->executeflags;   break;
        case 'I': destval = &template->sourcefile;     break;
        default:
            con_printmsg(LVL_ERROR, __FILE__, __LINE__, "internal error",
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
        con_printmsg(LVL_ERROR, file, line, "compile error",
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
    
    /*
     * Value will contain a newline character at the end, we need to strip
     * this otherwise kaboom, seriously, kaboom :P
     */
    *strrchr(value, '\n')='\0';
    
    /*
     * Now allocate and set the actual value for the specific tag. Which
     * was properly selected and can be accessed with *destval.
     */
    *destval = util_strdup(value);
    
    return true;
}

bool task_template_parse(const char *file, task_template_t *template, FILE *fp) {
    char  *data = NULL;
    char  *back = NULL;
    size_t size = 0;
    size_t line = 1;
    
    if (!template)
        return false;
    
    /* top down parsing */
    while (util_getline(&back, &size, fp) != EOF) {
        /* skip whitespace */
        data = back;
        if (*data && (*data == ' ' || *data == '\t'))
            data++;
            
        switch (*data) {
            /*
             * Handle comments inside task template files.  We're strict
             * about the language for fun :-)
             */
            case '/':
                if (data[1] != '/') {
                    con_printmsg(LVL_ERROR, file, line, "template parse error",
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
            case 'F':
            case 'S':
            case 'T':
            case 'C':
            case 'E':
            case 'I':
                if (data[1] != ':') {
                    con_printmsg(LVL_ERROR, file, line, "template parse error",
                        "expected `:` after `%c`",
                        *data
                    );
                    goto failure;
                }
                if (!task_template_generate(template, *data, file, line, &data[3])) {
                    con_printmsg(LVL_ERROR, file, line, "template compile error",
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
                    con_printmsg(LVL_ERROR, file, line, "template parse error",
                        "expected `:` after `%c`",
                        *data
                    );
                    goto failure;
                }
                
                if (value && *value && (*value == ' ' || *value == '\t'))
                    value++;
    
                /*
                 * Value will contain a newline character at the end, we need to strip
                 * this otherwise kaboom, seriously, kaboom :P
                 */
                *strrchr(value, '\n')='\0';
                
                vec_push(template->comparematch, util_strdup(value));
                
                break;
            }
            
            default:
                con_printmsg(LVL_ERROR, file, line, "template parse error",
                    "invalid tag `%c`", *data
                );
                goto failure;
            /* no break required */
        }
        
        /* update line and free old sata */
        line++;
        mem_d(back);
        back = NULL;
    }
    if (back)
        mem_d(back);
    return true;
    
failure:
    if (back)
        mem_d (back);
    return false;
}

/*
 * Nullifies the template data: used during initialization of a new
 * template and free.
 */
void task_template_nullify(task_template_t *template) {
    if (!template)
        return;
        
    template->description    = NULL;
    template->failuremessage = NULL;
    template->successmessage = NULL;
    template->proceduretype  = NULL;
    template->compileflags   = NULL;
    template->executeflags   = NULL;
    template->comparematch   = NULL;
    template->sourcefile     = NULL;
    template->tempfilename   = NULL;
}

task_template_t *task_template_compile(const char *file, const char *dir) {
    /* a page should be enough */
    char             fullfile[4096];
    FILE            *tempfile = NULL;
    task_template_t *template = NULL;
    
    memset  (fullfile, 0, sizeof(fullfile));
    snprintf(fullfile,    sizeof(fullfile), "%s/%s", dir, file);
    
    tempfile = fopen(fullfile, "r");
    template = mem_a(sizeof(task_template_t));
    task_template_nullify(template);
    
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
    
    if (!task_template_parse(file, template, tempfile)) {
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
    if (!template->description) {
        con_err("template compile error: %s missing `D:` tag\n", file);
        goto failure;
    }
    if (!template->proceduretype) {
        con_err("template compile error: %s missing `T:` tag\n", file);
        goto failure;
    }
    if (!template->compileflags) {
        con_err("template compile error: %s missing `C:` tag\n", file);
        goto failure;
    }
    if (!template->sourcefile) {
        con_err("template compile error: %s missing `I:` tag\n", file);
        goto failure;
    }
    
    /*
     * Now lets compile the template, compilation is really just
     * the process of validating the input.
     */
    if (!strcmp(template->proceduretype, "-compile")) {
        if (template->executeflags)
            con_err("template compile warning: %s erroneous tag `E:` when only compiling\n", file);
        if (template->comparematch)
            con_err("template compile warning: %s erroneous tag `M:` when only compiling\n", file);
        goto success;
    } else if (!strcmp(template->proceduretype, "-execute")) {
        if (!template->executeflags) {
            /* default to $null */
            template->executeflags = util_strdup("$null");
        }
        if (!template->comparematch) {
            con_err("template compile error: %s missing `M:` tag (use `$null` for exclude)\n", file);
            goto failure;
        }
    } else {
        con_err("template compile error: %s invalid procedure type: %s\n", file, template->proceduretype);
        goto failure;
    }
    
success:
    fclose(tempfile);
    return template;
    
failure:
    /*
     * The file might not exist and we jump here when that doesn't happen
     * so the check to see if it's not null here is required.
     */
    if (tempfile)
        fclose(tempfile);
    mem_d (template);
    
    return NULL;
}

void task_template_destroy(task_template_t **template) {
    if (!template)
        return;
        
    if ((*template)->description)    mem_d((*template)->description);
    if ((*template)->failuremessage) mem_d((*template)->failuremessage);
    if ((*template)->successmessage) mem_d((*template)->successmessage);
    if ((*template)->proceduretype)  mem_d((*template)->proceduretype);
    if ((*template)->compileflags)   mem_d((*template)->compileflags);
    if ((*template)->executeflags)   mem_d((*template)->executeflags);
    if ((*template)->sourcefile)     mem_d((*template)->sourcefile);
    
    /*
     * Delete all allocated string for task template then destroy the
     * main vector.
     */
    {
        size_t i = 0;
        for (; i < vec_size((*template)->comparematch); i++)
            mem_d((*template)->comparematch[i]);
            
        vec_free((*template)->comparematch);
    }
    
    /*
     * Nullify all the template members otherwise NULL comparision
     * checks will fail if template pointer is reused.
     */
    mem_d(*template);
}

/*
 * Now comes the task manager, this system allows adding tasks in and out
 * of a task list.  This is the executor of the tasks essentially as well.
 */
typedef struct {
    task_template_t *template;
    FILE           **runhandles;
    FILE            *stderrlog;
    FILE            *stdoutlog;
    char            *stdoutlogfile;
    char            *stderrlogfile;
    bool             compiled;
} task_t;

task_t *task_tasks = NULL;

/*
 * Read a directory and searches for all template files in it
 * which is later used to run all tests.
 */
bool task_propogate(const char *curdir) {
    bool             success = true;
    DIR             *dir;
    struct dirent   *files;
    struct stat      directory;
    char             buffer[4096];
    
    dir = opendir(curdir);
    
    while ((files = readdir(dir))) {
        memset  (buffer, 0,sizeof(buffer));
        snprintf(buffer,   sizeof(buffer), "%s/%s", curdir, files->d_name);
        
        if (stat(buffer, &directory) == -1) {
            con_err("internal error: stat failed, aborting\n");
            abort();
        }
        
        /* skip directories */
        if (S_ISDIR(directory.st_mode))
            continue;
            
        /*
         * We made it here, which concludes the file/directory is not
         * actually a directory, so it must be a file :)
         */
        if (strstr(files->d_name, ".tmpl") == &files->d_name[strlen(files->d_name) - (sizeof(".tmpl") - 1)]) {
            task_template_t *template = task_template_compile(files->d_name, curdir);
            char             buf[4096]; /* one page should be enough */
            task_t           task;
            
            util_debug("TEST", "compiling task template: %s/%s\n", curdir, files->d_name);
            if (!template) {
                con_err("error compiling task template: %s\n", files->d_name);
                success = false;
                continue;
            }
            /*
             * Generate a temportary file name for the output binary
             * so we don't trample over an existing one.
             */
            template->tempfilename = tempnam(curdir, "TMPDAT");
            
            /*
             * Generate the command required to open a pipe to a process
             * which will be refered to with a handle in the task for
             * reading the data from the pipe.
             */
            memset  (buf,0,sizeof(buf));
            snprintf(buf,  sizeof(buf), "%s %s/%s %s -o %s",
                task_bins[TASK_COMPILE],
                curdir,
                template->sourcefile,
                template->compileflags,
                template->tempfilename
            );
            
            /*
             * The task template was compiled, now lets create a task from
             * the template data which has now been propogated.
             */
            task.template = template;
            if (!(task.runhandles = task_popen(buf, "r"))) {
                con_err("error opening pipe to process for test: %s\n", template->description);
                success = false;
                continue;
            }
            
            util_debug("TEST", "executing test: `%s` [%s]\n", template->description, buf);
            
            /*
             * Open up some file desciptors for logging the stdout/stderr
             * to our own.
             */
            memset  (buf,0,sizeof(buf));
            snprintf(buf,  sizeof(buf), "%s.stdout", template->tempfilename);
            task.stdoutlogfile = util_strdup(buf);
            if (!(task.stdoutlog     = fopen(buf, "w"))) {
                con_err("error opening %s for stdout\n", buf);
                continue;
            }
            
            memset  (buf,0,sizeof(buf));
            snprintf(buf,  sizeof(buf), "%s.stderr", template->tempfilename);
            task.stderrlogfile = util_strdup(buf);
            if (!(task.stderrlog     = fopen(buf, "w"))) {
                con_err("error opening %s for stderr\n", buf);
                continue;
            }
            
            vec_push(task_tasks, task);
        }
    }
    
    closedir(dir);
    return success;
}

/*
 * Removes all temporary 'progs.dat' files created during compilation
 * of all tests'
 */
void task_cleanup(const char *curdir) {
    DIR             *dir;
    struct dirent   *files;
    char             buffer[4096];

    dir = opendir(curdir);
    
    while ((files = readdir(dir))) {
        memset(buffer, 0, sizeof(buffer));
        if (strstr(files->d_name, "TMP")) {
            snprintf(buffer, sizeof(buffer), "%s/%s", curdir, files->d_name);
            if (remove(buffer))
                con_err("error removing temporary file: %s\n", buffer);
            else
                util_debug("TEST", "removed temporary file: %s\n", buffer);
        }
    }
    
    closedir(dir);
}

/*
 * Task precleanup removes any existing temporary files or log files
 * left behind from a previous invoke of the test-suite.
 */
void task_precleanup(const char *curdir) {
    DIR             *dir;
    struct dirent   *files;
    char             buffer[4096];

    dir = opendir(curdir);
    
    while ((files = readdir(dir))) {
        memset(buffer, 0, sizeof(buffer));
        if (strstr(files->d_name, "TMP")     ||
            strstr(files->d_name, ".stdout") ||
            strstr(files->d_name, ".stderr"))
        {
            snprintf(buffer, sizeof(buffer), "%s/%s", curdir, files->d_name);
            if (remove(buffer))
                con_err("error removing temporary file: %s\n", buffer);
            else
                util_debug("TEST", "removed temporary file: %s\n", buffer);
        }
    }
    
    closedir(dir);
}

void task_destroy(const char *curdir) {
    /*
     * Free all the data in the task list and finally the list itself
     * then proceed to cleanup anything else outside the program like
     * temporary files.
     */
    size_t i;
    for (i = 0; i < vec_size(task_tasks); i++) {
        /*
         * Close any open handles to files or processes here.  It's mighty
         * annoying to have to do all this cleanup work.
         */
        if (task_tasks[i].runhandles) task_pclose(task_tasks[i].runhandles);
        if (task_tasks[i].stdoutlog)  fclose     (task_tasks[i].stdoutlog);
        if (task_tasks[i].stderrlog)  fclose     (task_tasks[i].stderrlog);
        
        /*
         * Only remove the log files if the test actually compiled otherwise
         * forget about it.
         */
        if (task_tasks[i].compiled) {
            if (remove(task_tasks[i].stdoutlogfile))
                con_err("error removing stdout log file: %s\n", task_tasks[i].stdoutlogfile);
            else
                util_debug("TEST", "removed stdout log file: %s\n", task_tasks[i].stdoutlogfile);
            
            if (remove(task_tasks[i].stderrlogfile))
                con_err("error removing stderr log file: %s\n", task_tasks[i].stderrlogfile);
            else
                util_debug("TEST", "removed stderr log file: %s\n", task_tasks[i].stderrlogfile);
        }
        
        /* free util_strdup data for log files */
        mem_d(task_tasks[i].stdoutlogfile);
        mem_d(task_tasks[i].stderrlogfile);
        
        task_template_destroy(&task_tasks[i].template);
    }
    vec_free(task_tasks);
    
    /*
     * Cleanup outside stuff like temporary files.
     */
    task_cleanup(curdir);
}

/*
 * This executes the QCVM task for a specificly compiled progs.dat
 * using the template passed into it for call-flags and user defined
 * messages.
 */
bool task_execute(task_template_t *template) {
    bool     success = false;
    FILE    *execute;
    char     buffer[4096];
    memset  (buffer,0,sizeof(buffer));
    
    /*
     * Drop the execution flags for the QCVM if none where
     * actually specified.
     */
    if (!strcmp(template->executeflags, "$null")) {
        snprintf(buffer,  sizeof(buffer), "%s %s",
            task_bins[TASK_EXECUTE],
            template->tempfilename
        );
    } else {
        snprintf(buffer,  sizeof(buffer), "%s %s %s",
            task_bins[TASK_EXECUTE],
            template->executeflags,
            template->tempfilename
        );
    }
    
    util_debug("TEST", "executing qcvm: `%s` [%s]\n",
        template->description,
        buffer
    );
    
    execute = popen(buffer, "r");
    if (!execute)
        return false;
        
    /*
     * Now lets read the lines and compare them to the matches we expect
     * and handle accordingly.
     */
    {
        char  *data    = NULL;
        size_t size    = 0;
        size_t compare = 0;
        while (util_getline(&data, &size, execute) != EOF) {
            if (!strcmp(data, "No main function found\n")) {
                con_err("test failure: `%s` [%s] (No main function found)\n",
                    template->description,
                    (template->failuremessage) ?
                    template->failuremessage : "unknown"
                );
                pclose(execute);
                return false;
            }
            
            /* 
             * Trim newlines from data since they will just break our
             * ability to properly validate matches.
             */
            if  (strrchr(data, '\n'))
                *strrchr(data, '\n') = '\0';
            
            
            /*
             * We only care about the last line from the output for now
             * implementing multi-line match is TODO.
             */
            success = !!!(strcmp(data, template->comparematch[compare++]));
        }
        mem_d(data);
        data = NULL;
    }
    pclose(execute);
    return success;
}

/*
 * This schedualizes all tasks and actually runs them individually
 * this is generally easy for just -compile variants.  For compile and
 * execution this takes more work since a task needs to be generated
 * from thin air and executed INLINE.
 */
void task_schedualize() {
    bool   execute  = false;
    char  *data     = NULL;
    size_t size     = 0;
    size_t i;
    
    for (i = 0; i < vec_size(task_tasks); i++) {
        /*
         * Generate a task from thin air if it requires execution in
         * the QCVM.
         */
        if (!strcmp(task_tasks[i].template->proceduretype, "-execute"))
            execute = true;
            
        /*
         * We assume it compiled before we actually compiled :).  On error
         * we change the value
         */
        task_tasks[i].compiled = true;
        
        /*
         * Read data from stdout first and pipe that stuff into a log file
         * then we do the same for stderr.
         */    
        while (util_getline(&data, &size, task_tasks[i].runhandles[1]) != EOF) {
            fputs(data, task_tasks[i].stdoutlog);
            fflush(task_tasks[i].stdoutlog);
        }
        while (util_getline(&data, &size, task_tasks[i].runhandles[2]) != EOF) {
            /*
             * If a string contains an error we just dissalow execution
             * of it in the vm.
             * 
             * TODO: make this more percise, e.g if we print a warning
             * that refers to a variable named error, or something like
             * that .. then this will blowup :P
             */
            if (strstr(data, "error")) {
                execute                = false;
                task_tasks[i].compiled = false;
            }
            
            fputs(data, task_tasks[i].stderrlog);
            fflush(task_tasks[i].stdoutlog);
        }
        
        /*
         * If we can execute we do so after all data has been read and
         * this paticular task has coupled execution in its procedure type
         */
        if (!execute)
            continue;
        
        /*
         * If we made it here that concludes the task is to be executed
         * in the virtual machine.
         */
        if (!task_execute(task_tasks[i].template)) {
            con_err("test failure: `%s` [%s] see %s.stdout and %s.stderr\n",
                task_tasks[i].template->description,
                (task_tasks[i].template->failuremessage) ?
                task_tasks[i].template->failuremessage : "unknown",
                task_tasks[i].template->tempfilename,
                task_tasks[i].template->tempfilename
            );
            continue;
        }
        
        con_out("test succeed: `%s` [%s]\n",
            task_tasks[i].template->description,
            (task_tasks[i].template->successmessage) ?
            task_tasks[i].template->successmessage  : "unknown"
        );
    }
    mem_d(data);
}

/*
 * This is the heart of the whole test-suite process.  This cleans up
 * any existing temporary files left behind as well as log files left
 * behind.  Then it propogates a list of tests from `curdir` by scaning
 * it for template files and compiling them into tasks, in which it
 * schedualizes them (executes them) and actually reports errors and
 * what not.  It then proceeds to destroy the tasks and return memory
 * it's the engine :)
 * 
 * It returns true of tests could be propogated, otherwise it returns
 * false.
 * 
 * It expects con_init() was called before hand.
 */
bool test_perform(const char *curdir) {
    task_precleanup(curdir);
    if (!task_propogate(curdir)) {
        con_err("error: failed to propogate tasks\n");
        task_destroy(curdir);
        return false;
    }
    /*
     * If we made it here all tasks where propogated from their resultant
     * template file.  So we can start the FILO scheduler, this has been
     * designed in the most thread-safe way possible for future threading
     * it's designed to prevent lock contention, and possible syncronization
     * issues.
     */
    task_schedualize();
    task_destroy(curdir);
    
    return true;
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
    char *redirout = (char*)stdout;
    char *redirerr = (char*)stderr;
    con_init();
    
    /*
     * Command line option parsing commences now We only need to support
     * a few things in the test suite.
     */
    while (argc > 1) {
        ++argv;
        --argc;

        if (argv[0][0] == '-') {
            if (parsecmd("redirout", &argc, &argv, &redirout, 1, false))
                continue;
            if (parsecmd("redirerr", &argc, &argv, &redirerr, 1, false))
                continue;
            
            con_change(redirout, redirerr);

            if (!strcmp(argv[0]+1, "debug")) {
                opts_debug = true;
                continue;
            }
            if (!strcmp(argv[0]+1, "memchk")) {
                opts_memchk = true;
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
    con_change(redirout, redirerr);
    test_perform("tests");
    util_meminfo();
    return 0;
}
