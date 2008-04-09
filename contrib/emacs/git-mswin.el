;;
;; Windows/msysgit specific modifications for git.el
;;
;; Problems running git.el on Windows/msysgit are resolved here, with
;; the eventual goal of updating upstream (contrib/emacs/git.el)
;;
;; Install this file by
;;   Inserting a (require 'git-mswin) in your .emacs
;; or
;;   Generating the auto-autoloads file via (batch-update-autoloads).
;;
;; Author: Clifford Caoile <piyo@users.sourceforge.net>
;;
;; License: none (public domain)

;; ----------------------------------------------------------------------
;; Autoloads
;; ----------------------------------------------------------------------

;;;###autoload (require 'git-mswin) ; force load

;; ----------------------------------------------------------------------
;; support routines
;; ----------------------------------------------------------------------

(require 'git)

;; Assume that git.cmd or git.exe is in the path provided to emacs.
;; If not, git.el itself wouldn't work in any case.

(defun msysgit-top-dir (path)
  "Returns the path of the msysgit top directory, based on PATH.
Usually PATH is set to exec-path."
  ;;FIXME: this should be memoized
  (let (result)
    (dolist (one-path path result)
      (when (null result)
        (if (or (string-match "\\bgit\\b.\\bcmd\\b$" one-path)
                (string-match "\\bgit\\b.\\bbin\\b$" one-path))
            (setq result (replace-regexp-in-string "....$" "" one-path)))))))

(defun msysgit--subpath-dir (path subpath)
  (let ((topdir (msysgit-top-dir path)))
    (if topdir
        (expand-file-name (concat topdir "/" subpath))
        nil)))

(defun msysgit-bin-dir      (path) (msysgit--subpath-dir path "bin"))
(defun msysgit-mingwbin-dir (path) (msysgit--subpath-dir path "mingw/bin"))

(defun msysgit-exec-path ()
  "Returns an alternate path consisting of only the bin and
mingw/bin directories. The result is suitable for setting to
exec-path, temporarily."
  (list (msysgit-bin-dir exec-path)
        (msysgit-mingwbin-dir exec-path)))


;; ----------------------------------------------------------------------
;; Code fixups
;; ----------------------------------------------------------------------

;; Emacs-lisp code that calls out to "git" on Windows fail on "git" if
;; the exec-path contains .../git/cmd. The following changes each
;; function's exec-path temporarily to call git.exe directly, in the
;; same way as git.cmd.

(defvar msysgit-wrapenv-functions
  '(git-call-process-env
    git-call-process-display-error
    ;; git-call-process-env-string g-c-p-env
    git-run-process-region
    ;; git-run-command-buffer      g-c-p-env
    git-run-hook
    git-get-top-dir
    ;; git-append-to-ignore        g-c-p-env
    ;; git-rev-parse               g-c-p-env-string
    ;; git-config                  g-c-p-env-string
    ;; git-symbolic-ref            g-c-p-env-string
    ;; git-update-ref              g-c-p-d-error
    ;; git-read-tree               g-c-p-env
    ;; git-write-tree              g-c-p-env-string
    git-empty-db-p
    ;; git-get-commit-description  g-c-p-env-string
    ;; git-run-diff-index          g-c-p-env
    ;; git-run-ls-files            g-c-p-env
    ;; git-run-ls-files-cached     g-c-p-env
    ;; git-run-ls-unmerged         g-c-p-env
    ;; git-update-index            g-c-p-env
    ;; git-do-commit               g-c-p-env
    ;; git-add-file                g-c-p-d-error
    ;; git-remove-file             g-c-p-d-error
    ;; git-revert-file             g-c-p-d-error
    ;; git-resolve-file            g-c-p-d-error
    ;; git-setup-commit-buffer     g-c-p-env
    ;; git-get-commit-files        g-c-p-env
    ;; git-amend-commit            g-c-p-d-error
    ;; git-refresh-status          g-c-p-env
    ;; git-update-saved-file       g-c-p-env
    )
  "List of functions that call out to git directly.
They will need to have their exec-path changed.")

(defmacro msysgit--defadvice-exec-path-wrap (func-as-sym)
  "Create a defadvice for function FUNC-AS-SYM that replaces the
exec-path for duration it is run.

The created macro when partially expanded looks like this:

 (defadvice git-get-top-dir (around msysgit-get-top-dir compile activate)
  (let ((exec-path  (msysgit-exec-path)))
         ad-do-it))"
  (let* ((t-msys-adv-name (make-symbol (concat "msysgit-" (symbol-name func-as-sym)))))
    `(defadvice ,(eval func-as-sym) (around ,t-msys-adv-name compile activate)
       (let ((exec-path  (msysgit-exec-path)))
         ad-do-it))))

;; Activate the defadvice
(dont-compile ; avoid "Error: Symbol's value as variable is void: func"
  (dolist (func msysgit-wrapenv-functions)
    (msysgit--defadvice-exec-path-wrap func)))


;; Functions that call out to "env" do not work on Windows.
;; We fake the env command by setting the emacs process' own
;; environment variables, temporarily.
;; FIXME: See git.git f27e55864317611385be4d33b3c53ca787379df9 and fix
;; up these functions accordingly (ie, delete these advices).

(defadvice git-run-command-region (around msysgit-run-command-region compile activate)
  (let ((myenv (ad-get-arg 3))
        restorer)
    (dolist (envvar myenv)
      (let ((varname (car envvar))
            (varval  (cdr envvar)))
        (add-to-list 'restorer (cons varname (getenv varname)))
        (setenv varname varval)))
    (ad-set-arg 3 nil)
    ad-do-it
    (dolist (envvar restorer)
      (setenv (car envvar) (cdr envvar)))))

(defadvice git-run-hook (around msysgit-run-hook compile activate)
  (let ((myenv (ad-get-arg 1))
        restorer)
    (dolist (envvar myenv)
      (let ((varname (car envvar))
            (varval  (cdr envvar)))
        (add-to-list 'restorer (cons varname (getenv varname)))
        (setenv varname varval)))
    (ad-set-arg 1 nil)
    ad-do-it
    (dolist (envvar restorer)
      (setenv (car envvar) (cdr envvar)))))


(provide 'git-mswin)
;; ends
