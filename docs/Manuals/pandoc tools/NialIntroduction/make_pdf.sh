#!/bin/sh
#!/bin/sh
cat PdfHeader.md  NialIntroduction.md > NI.md
pandoc NI.md -o NialIntroduction.pdf
cp NialIntroduction.pdf ../../pdf
rm NI.md
exit 0
