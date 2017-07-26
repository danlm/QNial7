;;; nial-console.el --- integration with the nial console.

;; Copyright (C) 2012 Zachary Elliott
;; Copyright (C) 2012 Michal J Wallace
;;
;; Authors: Michal J Wallace <michal.wallace@gmail.com>
;; Authors: Zachary Elliott <ZacharyElliott1@gmail.com>
;; URL: https://github.com/tangentstorm/nial-mode
;; Version: 1.1.0
;; Keywords: nial, Langauges

;; This file is not part of GNU Emacs.

;;; Commentary:

;; This code was forked from j-mode by Zachary Elliott.
;; (And, other than whitespace and the language name,
;; is still almost exactly the same.)

;;; License:

;; This program is free software; you can redistribute it and/or modify it under
;; the terms of the GNU General Public License as published by the Free Software
;; Foundation; either version 3 of the License, or (at your option) any later
;; version.
;;
;; This program is distributed in the hope that it will be useful, but WITHOUT
;; ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
;; FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
;; details.
;;
;; You should have received a copy of the GNU General Public License along with
;; GNU Emacs; see the file COPYING.  If not, write to the Free Software
;; Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
;; USA.

;;; Code:

(require 'comint)


;;--  customization----

(defgroup nial-console nil
  "REPL integration extention for `nial-mode'"
  :group 'applications
  :group 'nial
  :prefix "nial-console-")

(defcustom nial-console-cmd "/home/johng/bin/qnial7"
  "Name of the executable used for the nial session"
  :type 'string
  :group 'nial-console)

(defcustom nial-console-cmd-args '()
  "Arguments to be passed to the j-console-cmd on start"
  :type 'string
  :group 'nial-console)

(defcustom nial-console-cmd-init-file nil
  "Full path to the file who's contents are sent to the
  nial-console-cmd on start

Should be NIL if there is no file, not the empty string"
  :type 'string
  :group 'nial-console)

(defcustom nial-console-cmd-buffer-name "*Nial*"
  "Name of the buffer which contains the nial-console-cmd session"
  :type 'string
  :group 'nial-console)

(defvar nial-console-comint-input-filter-function nil
  "J mode specific mask for comint input filter function")

(defvar nial-console-comint-output-filter-function nil
  "J mode specific mask for comint output filter function")

(defvar nial-console-comint-preoutput-filter-function nil
  "J mode specific mask for comint preoutput filter function")


;;-- session interaction ---

(defun nial-console-create-session ()
  "Starts a comint session wrapped around the nial-console-cmd"
  (setq comint-process-echoes t)
  (apply 'make-comint nial-console-cmd-buffer-name
         nial-console-cmd nial-console-cmd-init-file
	 nial-console-cmd-args)
  (mapc
   (lambda (comint-hook-sym)
     (let ((local-comint-hook-fn-sym
            (intern
             (replace-regexp-in-string
              "s$" "" (concat "nial-console-"
			      (symbol-name comint-hook-sym))))))
       (when (symbol-value local-comint-hook-fn-sym)
         (add-hook comint-hook-sym
		   (symbol-value local-comint-hook-fn-sym)))))
   '(comint-input-filter-functions
     comint-output-filter-functions
     comint-preoutput-filter-functions)))

(defun nial-console-ensure-session ()
  "Checks for a running nial-console-cmd comint session and either
  returns it or starts a new session and returns that"
  (or (get-process nial-console-cmd-buffer-name)
      (progn
        (nial-console-create-session)
        (get-process nial-console-cmd-buffer-name))))


;;-- buffer integration ---

(define-derived-mode inferior-nial-mode comint-mode "Inferior Nial"
  "Major mode for Nial inferior process.")

;;;###autoload
(defun nial-console ()
  "Ensures a running nial-console-cmd session and switches focus to
the containing buffer"
  (interactive)
  (switch-to-buffer-other-window
   (process-buffer (nial-console-ensure-session)))
  (inferior-nial-mode))

(defun nial-console-execute-region ( start end )
  "Sends current region to the nial-console-cmd session and exectues it"
  (interactive "r")
  (when (= start end)
    (error "Region is empty"))
  (let ((region (buffer-substring-no-properties start end))
        (session (nial-console-ensure-session)))
    (pop-to-buffer (process-buffer session))
    (goto-char (point-max))
    (insert-string (format "\n%s\n" region))
    (comint-send-input)))

(defun nial-console-execute-line ()
  "Sends current line to the nial-console-cmd session and exectues it"
  (interactive)
  (nial-console-execute-region (point-at-bol) (point-at-eol)))

(defun nial-console-execute-buffer ()
  "Sends current buffer to the nial-console-cmd session and exectues it"
  (interactive)
  (nial-console-execute-region (point-min) (point-max)))

;;-- initialization ---

(provide 'nial-console)

;;; nial-console.el ends here
