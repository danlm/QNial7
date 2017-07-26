#!/bin/sh
#!/bin/sh
cat EpubHeader.md  V6LanguageDefinition.md > LD.md
pandoc LD.md -o V6LanguageDefinition.epub
cp V6LanguageDefinition.epub ../../epub
rm LD.md
exit 0
