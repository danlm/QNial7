#!/bin/sh
cat HtmlHeader.md  NialIntroduction.md > NI.md
pandoc -s --toc -c Styles.css -o NialIntroduction.html NI.md
cp NialIntroduction.html Styles.css ../../html
rm NI.md
exit 0
