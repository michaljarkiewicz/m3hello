#define _GNU_SOURCE
#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/pam_ext.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static const char *get_arg(int argc, const char **argv, const char *prefix, const char *defval) {
    size_t n = strlen(prefix);
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], prefix, n) == 0) return argv[i] + n;
    }
    return defval;
}

static int run_helper_and_capture(pam_handle_t *pamh, const char *helper, const char *user, char *out, size_t outsz) {
    int pipefd[2];
    if (pipe(pipefd) != 0) return -1;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]); close(pipefd[1]);
        return -1;
    }

    if (pid == 0) {
        // child
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);

        // wycisz stderr helpera
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) dup2(devnull, STDERR_FILENO);

        setenv("M3HELLO_QUIET", "1", 1);

        // helper open USER
        char *const args[] = { (char*)helper, (char*)"open", (char*)user, NULL };
        execv(helper, args);
        _exit(127);
    }

    // parent
    close(pipefd[1]);

    size_t used = 0;
    while (1) {
        ssize_t r = read(pipefd[0], out + used, (outsz - 1) - used);
        if (r == 0) break;
        if (r < 0) {
            if (errno == EINTR) continue;
            break;
        }
        used += (size_t)r;
        if (used >= outsz - 1) break;
    }
    out[used] = '\0';
    close(pipefd[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return -1;

    // trim CR/LF
    while (used > 0 && (out[used-1] == '\n' || out[used-1] == '\r')) {
        out[used-1] = '\0';
        used--;
    }
    return (used > 0) ? 0 : -1;
}

PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv) {
    (void)flags;

    const char *user = NULL;
    int rc = pam_get_user(pamh, &user, NULL);
    if (rc != PAM_SUCCESS || !user || !*user) return PAM_IGNORE;

    // Jeśli ktoś podał hasło normalnie, nie przeszkadzamy.
    const void *authtok_v = NULL;
    if (pam_get_item(pamh, PAM_AUTHTOK, &authtok_v) == PAM_SUCCESS) {
        const char *authtok = (const char*)authtok_v;
        if (authtok && *authtok) return PAM_IGNORE;
    }

    const char *helper = get_arg(argc, argv, "helper=", "/usr/local/bin/m3hello-vault");

    pam_info(pamh, "M3HELLO: dotknij klucza YubiKey, aby odblokowac keyring…");

    char pw[4096];
    memset(pw, 0, sizeof(pw));
    if (run_helper_and_capture(pamh, helper, user, pw, sizeof(pw)) != 0) {
        // Nie blokujemy logowania; po prostu keyring zostanie Locked i poprosi o hasło.
        return PAM_IGNORE;
    }

    pam_set_item(pamh, PAM_AUTHTOK, pw);
    memset(pw, 0, sizeof(pw));
    return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv) {
    (void)pamh; (void)flags; (void)argc; (void)argv;
    return PAM_SUCCESS;
}
