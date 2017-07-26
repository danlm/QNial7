#!/bin/sh
#!/bin/sh
cat PdfHeader.md  V6LanguageDefinition.md > LD.md
pandoc LD.md -o V6LanguageDefinition.pdf
cp V6LanguageDefinition.pdf ../../pdf
rm LD.md
exit 0

