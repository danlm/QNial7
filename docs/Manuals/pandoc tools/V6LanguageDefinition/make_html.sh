#!/bin/sh
cat HtmlHeader.md  V6LanguageDefinition.md > LD.md
pandoc -s --toc -c V6LanguageDefinition.css -o V6LanguageDefinition.html LD.md
cp V6LanguageDefinition.html V6LanguageDefinition.css ../../html
rm LD.md
exit 0
