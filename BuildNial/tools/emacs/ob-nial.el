;;; ob-nial.el --- org-babel functions for nial evaluation

;; Copyright (C) michal j wallace

;; Author: michal j wallace <http://tangentstorm.com>
;; Keywords: literate programming, nial
;; Homepage: https://github.com/tangentstorm/nial-mode
;; Version: 0.01

;;; License:

;; This program is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 3, or (at your option)
;; any later version.
;;
;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.
;;
;; You should have received a copy of the GNU General Public License
;; along with GNU Emacs; see the file COPYING.  If not, write to the
;; Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
;; Boston, MA 02110-1301, USA.

;;; Commentary:

;; Based on the ob-template.el example (except that ob-template
;; has out-of-date ideas about the order of the parameters, so
;; I replaced its 'first, 'second, etc calls with 'assoc...

;; If you are planning on adding a language to org-babel we would ask
;; that if possible you fill out the FSF copyright assignment form
;; available at http://orgmode.org/request-assign-future.txt as this
;; will make it possible to include your language support in the core
;; of Org-mode, otherwise unassigned language support files can still
;; be included in the contrib/ directory of the Org-mode repository.

;;; Code:
;-- requirements


(require 'ob)
;(require 'ob-ref)
;(require 'ob-comint)
;(require 'ob-eval)
(require 'nial-mode)
(require 'nial-console)



;-- helper routine for lisp

(defun getitem (alist key)
  (cdr (assoc key alist)))

(add-to-list 'org-babel-tangle-lang-exts '("nial" . "ndf"))

(defvar org-babel-default-header-args:nial '())


(defun org-babel-expand-body:nial (body params)
  "Expand BODY according to PARAMS, return the expanded body."
  (let ((vars (list (getitem params :var)))
	(set! (lambda (pair)
		(format "%s := %S;\n" (car pair)
			(org-babel-nial-var-to-nial (cdr pair))))))
    (concat (mapconcat set! vars "")
	    body)))

;; This is the main function which is called to evaluate a code
;; block.
;;
;; This function will evaluate the body of the source code and
;; return the results as emacs-lisp depending on the value of the
;; :results header argument
;; - output means that the output to STDOUT will be captured and
;;   returned
;; - value means that the value of the last statement in the
;;   source code block will be returned
;;
;;  header arguments specified by the user will be in `params`.

(defun org-babel-execute:nial (body params)
  "Execute a block of Nial code with org-babel.
This function is called by org-babel-execute-src-block'"
  (message "executing Nial source code block")

  (let* ((processed-params (org-babel-process-params params))
	 (sesname (getitem params :session))
         (session (org-babel-nial-initiate-session sesname))
	 (vars (getitem params :var))
         (full-body (org-babel-expand-body:nial
                     body processed-params)))

    (org-babel-nial-strip-whitespace
     (org-babel-nial-eval-string full-body))))


; -- (  TODO ) ---------------------------

;; This function should be used to assign any variables in params in
;; the context of the session environment.
(defun org-babel-prep-session:nial (session params)
  "Prepare SESSION according to the header arguments specified in PARAMS."
  )

(defun org-babel-nial-var-to-nial (var)
  "Convert an elisp var into a string of nial source code
specifying a var of the same value."
  var) ;; just pass through, until i can think of a better idea

(defun org-babel-nial-table-or-string (results)
  "If the results look like a table, then convert them into an
Emacs-lisp table, otherwise return the results as a string."
  ; TODO
  )

; -- comint -------------

(defun org-babel-nial-initiate-session (&optional session)
  "If there is not a current inferior-process-buffer in SESSION then create.
Return the initialized session."
  (let ((com (nial-console-ensure-session)))
	(comint-send-string com "setprompt ''\n") ))


(defun org-babel-nial-eval  ; mostly taken from ob-J.el
  "Sends STR to the `nial-console-cmd' session and exectues it."
  (let ((session (nial-console-ensure-session)))
    (with-current-buffer (process-buffer session)
      (goto-char (point-max))
      (insert (format "\n%s\n\r" str))
      (let ((beg (point)))
	(comint-send-input)
	(sit-for .1)
	(buffer-substring-no-properties
	 beg (point-max))))))


(defun org-babel-nial-strip-whitespace (str)
  "Remove whitespace from nial output STR."
  (with-temp-buffer
    (insert (delete ? str))
    (beginning-of-buffer)
    (while (looking-at "[[:cntrl:]]") (delete-char 1))

    ;; ;; delete whitespace up front:
    ;; (progn
    ;;	   (set-mark (point)) (forward-word)
    ;;	   (backward-word)
    ;;	   (delete-region (mark) (point)))
    (buffer-string)))

(provide 'ob-nial)
;;; ob-nial.el ends here
