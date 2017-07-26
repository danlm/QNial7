;;; nial-mode.el --- nial mode for emacs

; a simple mode for editing nial files
; based on tutorial from:
; http://www.emacswiki.org/emacs/ModeTutorial

;;; Commentary:
;;

;;; History:
;;

;;; Code:

(require 'cl) ; only for assert, afaik...


; define these for debugging just in case org isn't loaded
(defvar org-defvarialias)
(defvar org-babel-tangle-lang-exts)


(defvar nial-mode-hook nil)

(defvar nial-mode-map (make-keymap)
  "keymap for nial mode")

(add-to-list 'auto-mode-alist '("\\.ndf\\'" . nial-mode))


; ---- case-insensitive regexp builder ---

(defun str<= (x y) (or (string= x y) (string< x y)))
(assert (str<= "y" "z")) (assert (str<= "z" "z")) (assert (not (str<= "z" "y")))
(defun str-in-range-incl (s x y) (and (str<= x s) (str<= s y)))
(assert (str-in-range-incl "m" "b" "y"))

(defun nial-uncased-token (word)
  (let ((charptns (mapcar
		   (lambda (ch) (if (str-in-range-incl ch "a" "z")
				    (concat "[" ch (upcase ch) "]") ch))
		   (mapcar 'char-to-string (string-to-list
					    (regexp-quote word))))))
	(concat "" (apply 'concat charptns) "")))


(defun nial-uncased-regexp (words)
  (regexp-opt (mapcar 'nial-uncased-token words)))


(defun join-strings (parts glue)
  (mapconcat 'identity parts glue))
(assert (string= (join-strings (list "a" "b" "c") ",") "a,b,c"))

(defun make-optre (words)
  (concat "\\<"
	  "\\(?:" (join-strings (mapcar 'nial-uncased-token words) "\\|") "\\)"
	  ;(nial-uncased-regexp words)
	  "\\>"))


;; debug stuff
;; (regexp-opt (list "a[Bb]c" "123" "xyz"))
;; (nial-uncased-regexp (list "abc" "xyz"))
;; (regexp-opt (mapcar 'nial-uncased-token-regexp (list "abc" "xyz")) 'symbols)
;; (mapcar 'nial-uncased-token-regexp (list "abc" "xyz"))
;; (join-strings (mapcar 'nial-uncased-token (list "abc" "xyz")) "\\|")
;; (make-optre (list "abc" "xyz"))



; -- faces --

(defface nial-punctuation-face
  '((default (:foreground "magenta" )))
  "Face for punctuation."
  :group 'nial-faces)

(defface nial-fault-face
  '((default (:foreground "brightred" )))
  "Face for ?faults."
  :group 'nial-faces)

(defface nial-symbol-face
  '((default (:foreground "#ffaf00" )))
  "Face for \"symbols."
  :group 'nial-faces)

(defface nial-transformer-face
  '((default (:foreground "yellow" )))
  "Face for built-in transformers."
  :group 'nial-faces)

; these seem to be required for the font-lock-defaults
(defvar nial-fault-face 'nial-fault-face "Face for ?faults.")
(defvar nial-punctuation-face 'nial-punctuation-face "Face for punctuation.")
(defvar nial-symbol-face 'nial-symbol-face "Face for \"symbols.")
(defvar nial-transformer-face 'nial-transformer-face "Face for transformers")


;-- syntax highlighting --

(defconst nial-font-lock-keywords
  (list

   ; keywords
   (cons (make-optre
	  '("is" "gets" "op" "tr"  ";"
	    "if" "then" "elseif" "else" "endif"
	    "case"  "from" "endcase"
	    "begin" "end"
	    "for"  "with" "endfor"
	    "while" "do" "endwhile"
	    "repeat" "until" "endrepeat"))
	 font-lock-keyword-face)

   ; punctuation
   (cons (make-optre '(":="))
	 'nial-punctuation-face)

   ; operators
   (cons (make-optre
	  '("." "(" "!" "#" ")" "," "+" "*" "-" "<<"
	    "/" "<" ">>" "<=" ">" "=" ">=" "@" "["
	    "]" "{" "}" "|" "~=" ))
	 font-lock-builtin-face)

   ; "symbols
   (cons "\"[[:word:]]+" 'nial-symbol-face)

   ; ?faults
   (cons "\?[[:word:]]+" 'nial-fault-face)

   ; predefined words
   ;
   ; there seems to be a limit on how big a regexep can be, so
   ; i broke this down into smaller chunks. (putting them together
   ; just caused the match to fail silently

   (cons (make-optre
	  '("accumulate" "across"
	    "bycols" "bykey"  "byrows"
	    "converse"  "down"
	    "eachboth" "eachall" "each"
            "eachleft" "eachright"
	    "filter" "fold" "fork"
	    "grade"  "inner" "iterate"
	    "leaf"  "no_tr" "outer"
	    "partition" "rank" "recur"
	    "reduce" "reducecols" "reducerows"
	    "sort" "team" "timeit" "twig"))
	    'nial-transformer-face)

   (cons (make-optre '(
	 "operation" "expression"
	 "and" "abs" "allbools"  "allints"
	 "allchars" "allin" "allreals" "allnumeric" "append"
	 "arcsin" "arccos" "appendfile" "apply" "arctan" "atomic"
	 "assign" "atversion" "axes" "cart"  "break" "blend"
	 "breaklist" "breakin" "bye"  "callstack"
	 "choose" "char" "ceiling" "catenate" "charrep" "check_socket"
	 "cos" "content" "close" "clearws" "clearprofile" "cols"
	 "continue" "copyright" "cosh" "cull" "count"
	 "diverse" "deepplace" "cutall" "cut" "display" "deparse"
	 "deepupdate" "descan" "depth" "diagram" "div" "divide"
	 "drop" "dropright""edit"
	 "empty" "expression" "exit"
	 "except" "erase" "equal" "eval" "eraserecord" "execute"
	 "exp" "external" "exprs" "findall" "find" "false" "fault"
	 "falsehood" "filestatus" "filelength" "filepath" "filetally"
	 "floor" "first" "flip" "from"  "fuse"
	 "fromraw" "front" "gage" "getfile" "getdef" "getcommandline"
	 "getenv" "getname" "hitch" "grid" "getsyms" "gradeup"
	 "gt" "gte" "host" "in" "inverse" "innerproduct" "inv"
	 "ip" "ln" "link" "isboolean" "isinteger" "ischar" "isfault"
         "isreal" "isphrase" "isstring" "istruthvalue"
         "last" "laminate" "like" "libpath" "library" "list" "load"
         "loaddefs" "nonlocal" "max" "match" "log" "lt" "lower" "lte"
	 "mate" "min" "maxlength" "mod" "mix" "minus" "nialroot" "mold"

	 "not" "numeric" "null" "no_op" "no_expr" "notin"
	 "operation" "open" "or" "opposite" "opp" "ops" "plus" "pick"
	 "pack" "pass" "pair" "parse" "paste"
	 "phrase" "pi" "place" "picture" "placeall" "power" "positions"
	 "post" "quotient" "putfile" "profile" "prod" "product"
	 "profiletree" "profiletable" "quiet_fault" "raise" "reach"
	 "random" "reciprocal" "read" "readfile" "readchar"
	 "readarray" "readfield" "readscreen" "readrecord" "recip"
	 "reshape" "seek"
	 "second" "rest" "reverse" "restart" "return_status" "scan"
	 "save" "rows" "rotate" "seed" "see" "sublist" "sin" "simple"
	 "shape" "setformat" "setdeftrace" "set" "seeusercalls"
	 "seeprimcalls" "separator" "setwidth" "settrigger" "setmessages"

	 "setlogname" "setinterrupts" "setprompt" "setprofile" "sinh"
	 "single" "sqrt" "solitary" "sketch" "sleep" "socket_listen"
	 "socket_accept" "socket_close" "socket_bind"
	 "socket_connect" "socket_getline" "socket_receive"
	 "socket_peek" "socket_read" "socket_send" "socket_write"
	 "solve" "split" "sortup" "string" "status" "take"
	 "symbols" "sum" "system" "tan" "tally" "takeright" "tanh"
	 "tell" "tr" "times" "third" "time"
	 "toupper" "tolower" "timestamp" "tonumber"

	 "toraw" "toplevel" "transformer" "type" "transpose"
	 "true" "trs" "truth" "unequal" "variable"
	 "valence" "up" "updateall" "update" "vacate" "value"

	 "version" "vars" "void"
         "watch" "watchlist"
	 "write" "writechars" "writearray" "writefile"
	 "writefield" "writescreen" "writerecord"

	 ))
	 'font-lock-function-name-face)
   ) "highlighting for nial mode")


; syntax table
(defun nial-update-syntax-table (group chars)
  (mapcar (lambda (c) (modify-syntax-entry c group nial-mode-syntax-table))
	  chars))

(defvar nial-mode-syntax-table
  (let ((nial-mode-syntax-table (make-syntax-table)))

    ; underscores okay in names
    (modify-syntax-entry ?_ "w" nial-mode-syntax-table)

    ;; Add operator symbols misassigned in the std table
    (nial-update-syntax-table "w"
	'(?& ?: ?* ?+ ?- ?/ ?< ?= ?> ?| ?~))

    ; comments
    (modify-syntax-entry ?% "<" nial-mode-syntax-table)
    (modify-syntax-entry ?# "<" nial-mode-syntax-table)
    (modify-syntax-entry ?\n ">" nial-mode-syntax-table)

    ; ' starts a string
    (modify-syntax-entry ?' "\"" nial-mode-syntax-table)

    ; " represents a symbol
    (modify-syntax-entry ?\" "'" nial-mode-syntax-table)

    nial-mode-syntax-table)
  "syntax table for nial-mode")


(defvar nial-indent 4)

; @TODO: define logic for un-indenting!
(defun nial-indent-line ()
  "indent current line as nial code"
  (interactive)
  (beginning-of-line)
  (if (bobp)
      (indent-line-to 0) ; beginning of buffer
    (let ((not-indented t) cur-indent)
      (progn
	(save-excursion
	  (forward-line -1)
	  (setq cur-indent (+ (current-indentation) nial-indent)))))))




(defun nial-mode ()
  "major mode for nial"
  (interactive)
  (kill-all-local-variables)
  (set-syntax-table nial-mode-syntax-table)
  (use-local-map nial-mode-map)
  (set (make-local-variable 'font-lock-defaults)  '(nial-font-lock-keywords))
  (set (make-local-variable 'indent-line-function) 'nial-indent-line)
  (setq major-mode 'nial-mode)
  (setq mode-name "Nial")
  (run-hooks 'nial-mode-hook))


(provide 'nial-mode)

;;; nial-mode.el ends here
