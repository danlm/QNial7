#!/bin/sh
#!/bin/sh
cat EpubHeader.md  NialIntroduction.md > NI.md
pandoc NI.md -o NialIntroduction.epub
cp NialIntroduction.epub ../../epub
rm NI.md
exit 0
