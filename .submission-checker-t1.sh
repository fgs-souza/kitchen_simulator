#!/bin/bash
# Usage: grade dir_or_archive [output]

# Ensure realpath 
realpath . &>/dev/null
HAD_REALPATH=$(test "$?" -eq 127 && echo no || echo yes)
if [ "$HAD_REALPATH" = "no" ]; then
  cat > /tmp/realpath-grade.c <<EOF
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
  char* path = argv[1];
  char result[8192];
  memset(result, 0, 8192);

  if (argc == 1) {
      printf("Usage: %s path\n", argv[0]);
      return 2;
  }
  
  if (realpath(path, result)) {
    printf("%s\n", result);
    return 0;
  } else {
    printf("%s\n", argv[1]);
    return 1;
  }
}
EOF
  cc -o /tmp/realpath-grade /tmp/realpath-grade.c
  function realpath () {
    /tmp/realpath-grade $@
  }
fi

INFILE=$1
if [ -z "$INFILE" ]; then
  CWD_KBS=$(du -d 0 . | cut -f 1)
  if [ -n "$CWD_KBS" -a "$CWD_KBS" -gt 20000 ]; then
    echo "Chamado sem argumentos."\
         "Supus que \".\" deve ser avaliado, mas esse diretório é muito grande!"\
         "Se realmente deseja avaliar \".\", execute $0 ."
    exit 1
  fi
fi
test -z "$INFILE" && INFILE="."
INFILE=$(realpath "$INFILE")
# grades.csv is optional
OUTPUT=""
test -z "$2" || OUTPUT=$(realpath "$2")
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
# Absolute path to this script
THEPACK="${DIR}/$(basename "${BASH_SOURCE[0]}")"
STARTDIR=$(pwd)

# Split basename and extension
BASE=$(basename "$INFILE")
EXT=""
if [ ! -d "$INFILE" ]; then
  BASE=$(echo $(basename "$INFILE") | sed -E 's/^(.*)(\.(c|zip|(tar\.)?(gz|bz2|xz)))$/\1/g')
  EXT=$(echo  $(basename "$INFILE") | sed -E 's/^(.*)(\.(c|zip|(tar\.)?(gz|bz2|xz)))$/\2/g')
fi

# Setup working dir
rm -fr "/tmp/$BASE-test" || true
mkdir "/tmp/$BASE-test" || ( echo "Could not mkdir /tmp/$BASE-test"; exit 1 )
UNPACK_ROOT="/tmp/$BASE-test"
cd "$UNPACK_ROOT"

function cleanup () {
  test -n "$1" && echo "$1"
  cd "$STARTDIR"
  rm -fr "/tmp/$BASE-test"
  test "$HAD_REALPATH" = "yes" || rm /tmp/realpath-grade* &>/dev/null
  return 1 # helps with precedence
}

# Avoid messing up with the running user's home directory
# Not entirely safe, running as another user is recommended
export HOME=.

# Check if file is a tar archive
ISTAR=no
if [ ! -d "$INFILE" ]; then
  ISTAR=$( (tar tf "$INFILE" &> /dev/null && echo yes) || echo no )
fi

# Unpack the submission (or copy the dir)
if [ -d "$INFILE" ]; then
  cp -r "$INFILE" . || cleanup || exit 1 
elif [ "$EXT" = ".c" ]; then
  echo "Corrigindo um único arquivo .c. O recomendado é corrigir uma pasta ou  arquivo .tar.{gz,bz2,xz}, zip, como enviado ao moodle"
  mkdir c-files || cleanup || exit 1
  cp "$INFILE" c-files/ ||  cleanup || exit 1
elif [ "$EXT" = ".zip" ]; then
  unzip "$INFILE" || cleanup || exit 1
elif [ "$EXT" = ".tar.gz" ]; then
  tar zxf "$INFILE" || cleanup || exit 1
elif [ "$EXT" = ".tar.bz2" ]; then
  tar jxf "$INFILE" || cleanup || exit 1
elif [ "$EXT" = ".tar.xz" ]; then
  tar Jxf "$INFILE" || cleanup || exit 1
elif [ "$EXT" = ".gz" -a "$ISTAR" = "yes" ]; then
  tar zxf "$INFILE" || cleanup || exit 1
elif [ "$EXT" = ".gz" -a "$ISTAR" = "no" ]; then
  gzip -cdk "$INFILE" > "$BASE" || cleanup || exit 1
elif [ "$EXT" = ".bz2" -a "$ISTAR" = "yes"  ]; then
  tar jxf "$INFILE" || cleanup || exit 1
elif [ "$EXT" = ".bz2" -a "$ISTAR" = "no" ]; then
  bzip2 -cdk "$INFILE" > "$BASE" || cleanup || exit 1
elif [ "$EXT" = ".xz" -a "$ISTAR" = "yes"  ]; then
  tar Jxf "$INFILE" || cleanup || exit 1
elif [ "$EXT" = ".xz" -a "$ISTAR" = "no" ]; then
  xz -cdk "$INFILE" > "$BASE" || cleanup || exit 1
else
  echo "Unknown extension $EXT"; cleanup; exit 1
fi

# There must be exactly one top-level dir inside the submission
# As a fallback, if there is no directory, will work directly on 
# tmp/$BASE-test, but in this case there must be files! 
function get-legit-dirs  {
  find . -mindepth 1 -maxdepth 1 -type d | grep -vE '^\./__MACOS' | grep -vE '^\./\.'
}
NDIRS=$(get-legit-dirs | wc -l)
test "$NDIRS" -lt 2 || \
  cleanup "Malformed archive! Expected exactly one directory, found $NDIRS" || exit 1
test  "$NDIRS" -eq  1 -o  "$(find . -mindepth 1 -maxdepth 1 -type f | wc -l)" -gt 0  || \
  cleanup "Empty archive!" || exit 1
if [ "$NDIRS" -eq 1 ]; then #only cd if there is a dir
  cd "$(get-legit-dirs)"
fi

# Unpack the testbench
tail -n +$(($(grep -ahn  '^__TESTBENCH_MARKER__' "$THEPACK" | cut -f1 -d:) +1)) "$THEPACK" | tar zx
cd testbench || cleanup || exit 1

# Deploy additional binaries so that validate.sh can use them
test "$HAD_REALPATH" = "yes" || cp /tmp/realpath-grade "tools/realpath"
export PATH="$PATH:$(realpath "tools")"

# Run validate
(./validate.sh 2>&1 | tee validate.log) || cleanup || exit 1

# Write output file
if [ -n "$OUTPUT" ]; then
  #write grade
  echo "@@@###grade:" > result
  cat grade >> result || cleanup || exit 1
  #write feedback, falling back to validate.log
  echo "@@@###feedback:" >> result
  (test -f feedback && cat feedback >> result) || \
    (test -f validate.log && cat validate.log >> result) || \
    cleanup "No feedback file!" || exit 1
  #Copy result to output
  test ! -d "$OUTPUT" || cleanup "$OUTPUT is a directory!" || exit 1
  rm -f "$OUTPUT"
  cp result "$OUTPUT"
fi

echo -e "Grade for $BASE$EXT: $(cat grade)"

cleanup || true

exit 0

__TESTBENCH_MARKER__
� 壝[ �=kW�Ȓ�ٿ�qc;�<½�<3�!�r'�����۠�-�J2�1��{��~��"l��!uK����g�sluuuuUuUuuI�h�Sox���W�:pmom����VG�+�o������M��ovֿ![_��䚇��3��0�������|~-X\�[����+-�������F�7F��������Z�|C:�GB��'�����s�[;w��ګ���r��?8<��/����_���7��������f���`��ݽ_��[5T��H}Yޭ�?� 1�ڈ^�y�ɤ6���#ˍ���,3�V���!��Yf#��YH���!;1�{P�}m�{��I#�F��Dtx��+?���NH�N�?�Z��38��'S��|Jf�|�?{��݌���K�3$�M#��J�K��8�K�UQJڎ�j�ޅ���rd+;$a9C���H����.!5v�S��7��=���C�J���Ff�?�����R������}��̀�2�}��+�w��
��ɻ�Z{醡�]�G�h6�@�&���Sp���ь)�;��E�j�~�C炾 �N��#b1��p>�l�����{0���w`7�;\�b�}����n4j}/�^D�`?~>8��ˍ� �H{p	Ʀ��?<y�d���SX�%?b�c�%���zI�Zۮ8�+�1��hxl������Npu�ѐ��]˒�dI��?_�k�(#�K 	`#�|�J����uoK(q�IH��(���M�M�L���~�e��߭޼:�wgT_��.L:��2>LX����ҠֳP*����AR�w���T5᫧��֭� W��U����d��#���j�����Q�*WV�L6Q��9,�_�F�V9�0<�<\/�������o�-�F��[|�֗����Q��,E�2��|2����) b��a�H�ʵ�1,7�_�����|��}�鬭]|k�Ɏf(j�r�����Lܑ�A;����^�@'�|s3��w;�ʹ���>�����coD�Ķ��{x��{v|b�N�~��b�b۵'����͇�=q��d�09�ų���x"�]���ѹ�zn� �hx�M��ի]��HAk�l�Q�����w6�`V�1������ 3�MY�ߘ�����!�F�^��@�|�l=�z�0�g�N4��Q#����,Y�*��?k��#qP��F`�ך��k�=��?�Ul���2F������������\��v}�r;aH�H��6f�{��{���4u��m�>J�xԍ ���&��=�ע�3��Kؾ�,�՜ɹ=s��k����$����᠙��pk׫���6[�}ך��r$�?�g�ӝ���)miTN}�;�zA�t��Й ���Ы��~&&f�����D�n[-��&����`m#�ʉ8T}���cg-L�@e�},�Mq��y�>lj��OF�����1i,aW��J�f?h�7ڤY6��]ߍ �F�fS�"w[��.�ՇMN�[ɏ�NV|�oyxLh)������"/u�ǜdwŖ_(�bQ�้*'��e���ݯK�Gݑ"�6V4�ra�HG ����=�*_H}�1�oq�(&{�����vʻ{��C�}<��!m�nU`����Þ>���i���9�������0Q�v��5
C���mPHh��%����.@��wd�O�}�}��_������ܔ�.���n�Z�y�*����w��cn_����ގ���=��b���_���9��yG�D� 
C��x2/�^�su�WҒ��"i�7,���bas(Gx�oa9O�>�BFKo��qF0dL\,U�֭5Dv+�p��4�kǰ�����ɀ1h�^�uU�S,��$��o)c8nN�qc�8g�)�Lo%����@ֺO���u��n>ů��s<�c_��"����[�^��
/��_-��ir����6ʅ�ȱPfCT+��F��c���������`�E�+Q2v�X�r�u�YE!��=��sf����L
 ��e�|n<��l	���w�7e �ܝ�F�$<���1��>Mu�vv0�au)�tz:�^�/6�j���m6�"��������F.vf���޸��e��'����d�����Fz��G���L���A�5��=8��#������)�ꮛ�NY�6� p����"o�m9���5�j_S�=|�M�!�zԞ���S?�h��%�3���h�n@'�:�L� )v�ѣ׌�܋ӉG��%'�k㞹�2�R|�n���rH0�����N�B�P!/&
��K'�����U��Ѿ�'�U�gP$������PH��H�P�]"9��"P�Jb�"���⦖���#�Q�0���}0�f}�u��שu���:uB�
2h�cb��"���k�<^���/]�o�/�5����h{)R�H�w�ca�3r�9-c˂�I|` rE��ɠ��1�u;��T�������Y�Zm��og1I��{^����>�3���7�Ϧ��g������������㿇�-{uf�v���/�oO�,5���ȧ���!uk�n�Vud�|��:��N9�a��V���'��x�o��Zfz��~���:�Kⶖ��n_ά��Z?��U���DŢ���tڷ�ק�������!���Ӄ7��}z��do�_�t����a�c������_���i�W���7�����|�q�?�e���:��d�����S�q�7&Kj�H���5��!�ƌtg��0�]?�{��K�M��QR�U����	3Ե���o��L��7^Mږ�N��%�a�-�'�,�h&i�Yd;ZB{xnGV�0�\4�ZD~F���CD�LO�v� |�F��dx.��ъ0���h�_{��)�\��ivlprr�G�I��
�CӁ�
"F���	#�L����Xx#l5$�$�r:y	�S}��g��S��-0j}��~��a>��JQ�ƪ��� TDT��潘�)��$д��eWC�s���mf��NTZ��b��������T�����������>����LUV�Ǫߢ,[�e�,�5�w��M׳���Ջie�149���
���
��h�%��;��[nht8B��&w�W�П{�)m�h������܃9ĭ��B@f��84��7����E���`4��B���O��%�,R�@>pMDs�s��	V��#悸Fı�NA.�2yO?�Q.$R�%r��n���h�5]҉�|}6xc��&���D����dw�F�,K�6'�lǿa����ا���w[����)��)�K�K�߉2���7!�)�KZ41[�b ���	訕,K��sj&E�DL���*+�y��Y�$g**���j����^�-Y<_��kMY���RO�+D`�^��98�����/�h��c���^�H-p$���H�^�Q�Y=�6Hb	��H�'c�m��q.��e2t��BQ���0�,$�,|v ��[��"�9^]ů��T��=��71��_{����ӳ���TlvS�U�o���F��}N,����3ꡂ]9���Ӑx>aV�<��חpO7��ϑ�Eb�1L�B�H���$�
}��:�>{�(>)���_���x8�� 8'���OK�Q���x��aZ���V W	Ā#��Ә�W����>�$CF�å����v�@TZ������6����U�����m�D=��|���#W�.�4�{�y�lN���`�ɒ��t����H�$���s�������0x[�d~��L�-��L!	�d��[����Dn�s�Ց3������D+2E�0��D�`�O���),G�j�3�`�$�T���(U������d������oK��JT��L<�j12)nO\o���c���<�<��!O�vA%�\)f
Ǫ�%�ʓZ��䫝����S��86�������g��btI�u�>�.�p��@V���{n��u۲����<\�d�y��e��.��fW��´���p�ar8L�.}�阢2�\�MA6Cl$�u���]�܈��u�B0��ۺ����>Pd���,���LЖ^Tͷ�7*��A䱃=���2������J;èBk^��0g�g^�P�(��4,��x��L�0	sX�2�݂�2��8Ip��Y��XD�&gv^�d�W O_�}�YGtJ���`K),��s1w����ޒ�U��0�.�{P!��)o��Zjd���S�����ע�m1��ȋ��`�U�P�� N&3��y:a��0+�+l\֓u,�A��3�i6񾤖1t��2Ã�xze�<f�Q���,�J�����%lo�)�3�8H����wK:%,y�m�?�̅��a��3?�1q*��D�pzd��;���Y��--C��;CD<��������*Zɉ���˨��ɫB]�@1R'Z��K|fqw3u�
ޜ=��dQh�|<�/H����4'rIw������Ás���7Y"^�B�61��0?7Fy�O
�S�j�6TO+�PS)Q�e��"�#uV��jQ(�a�1�[�S���x)�?2ߤ#�<dr�m�3��\Isg!L���7-h[��'`-=�s'�������w�j�,�ʪX��x��8��TL������'�`����jp���%%�4)�xoj%e7�v-�b�$_J+sNģG��978fe8��:m7U�:�
�����e%�e�\ƺ"�S��p>��`f�(�v�����[Tg	2=a_UoL�]p#�mw�bs��)����h��R	S��~�Ơ8�a�)�0@]N��0R�	�hn�d�m�O��Jew���-ɲ�ܙ�[�<��sک!ށ��F������&Q���qn�Tc���Y�'��2�%��Q�R��o�ac�f��)�(>tW�����w�u�WI�/�S�,���цl>�h)��&��fY����I 9��3�Щ� ̠�&R)Uz�E�iGW-��m�Uњ,��5F�8�4��*N~��m��a�'����� 6Um�ve�d�)uWs�u����n9J�k��{�(;!�4�<5e��$�g�����WC�s sF�d�Ͽ��܋ (w�(p�� �d;@ �5.�[)JE�A�����'�@�6|��fQ��d����^���LMQ�@ɤs��Rm�:�W��3u�T��ӨH�o_�v�OFcȣ���3���b�����	O��Pj�
�o%q���nR`�*�̸�R2�����
do#`�����T�����$n�rx�~0�r�$�f������Jc$���q�5�n���3�:�T@����:;{�ɱ�:�|�{%T����Ӹ��"OR���T�9A���!�T�똜�]��L�2�VB0���Y�%�)�`dn !�{��E�������'�@s]�ZD����ϯG�v����;��,�R���e̙[�Y��qf�oUK_���[	>Y���S��a�0��A��Mbȵ���坉Q�+i����[��0��ybc�@B������Ֆe"U�c�BNF�TO�������D�2�<�e:ChD&��jP���
"{Z9��8is�u�ԋ8��|�c��Ҩ>}���G<��C^ѓ�K�?��ec���c���V.6�j�X�����0�6��\��A��G���Y��B,��l�x�z��^��_��\�?=����`�=�0cEMHxVQ��Q- ��e�Q=��3d߻�Q��?�݇Hk�	��[u�*��*�t��!*C�V�R�I֤~n����w]=:a�M6�th��e���Q/׋$A�����2
�G�t��@${�1���Ej�B�">��:��rk�
ʾ��K'z���C�A�ke�����%�u����V�^�({�W��y�����?�2������r�U��4�E~	*���
�~T͗鹕����=����K��ꕧ3K��ўUQ�$��V�Wl� �j�,H3�E���d��ۺ���;�RƕO�I�\x��Q�����ҕx���k;d�f�.%�X{���bq��=<7�L]-e_�?c���*`�J3�$5��T�D�~���꙰J�A,��:8>xo�!Ve]�½���R&\fi�2�"�jC�5K]Ï����^���A~�qc+������G��W�[����۷�_kD���e:0i�)@K�9�	G��܋��4Rl�
7��X���%-9���Q�eK�����y�����qh��M�'���SGZ[2�����KA����u9��0��~�Zel�����|�i��=��}��p��.����� �u�Y)f6�lw�=�۩���ܵ2�V�g:�o�	kq��{�3B�������7R
���:&�VZ∱ܭPT�G_�Δ�Z8��_� IL�B�����9TҒZ���H���Bf\V����Cs��o{G��m|6,K�;���Irܑ#'�tv��W%9��uop$t�s<2�PR�~����x�On_�z����<�Ǟv�I,�X,�X,��ೇe`��A���ԛ��ʞﮪz3L/��gD#�^ng������H��vR���G�w��=���E[�K�/���mU�o%�jU?ɤ��㊄�Ű���c�c+�[���3H\��E}��b�o�9x�Y���\_k����m��Q_�sn���ܶςȯ9���}DH4,���q@սC��2���G�Æ�i���S��*"�;e�*j>�*]�\/�r��?�_�L�Q<w�����&���*�$B{�O��� �dI�H_ �1tԆv�̍"_���e�t|��␏��ҷ@?��I����	�3�~�;��P|:IС����/���=<g��ˈ<��n@�Gb�̦H!�	7������i��E��x�Y��I�I��`�� ��R�Ќ��=� o��,�V���5���u>����pƙ���ph��g�Ѓ��λ����W OXc�1|r����)S�#r�so<��SN4���쀙��4L�&�9f$bS����#Cg�&�YlM-�T�
]�U�x���C_�6r��p�� ��0;��}����S�V@�?��Hd?�o���z|J����	�8q�1~��<zS�B�{�)�� h
�5��e8b;���գ�ߎ��{z�s�`������xr0<�L� ���U��o�_C繌� �a�Ax~�kAְ�3!"!���/�p�u�D�f���1�������� �x���d}�]�qm��bf�A �A
$��c����X�둭��&�̖�g�`T�\�I���X�1ע��D L�2��-�!̃�K�]�A:N.��|�ۋ���l,�݃���u���RǓ�S�[�uS��|�� �#��>ak?]���������|oz%� �J�q��D[��-%wơ�F������?��,�+te�pH�i���8�=ş�-⷟G�<��'q�Q1R`�y�~��I�K�q,H�{�
"����f�r:�9�S��M��+����լ�G-�M|�*2L��+�{������.����.@�!�B��H�`[�UR:�s��,fD.�-��7�ꟲs#-��rl*~1�bͮ`a�#.��UgL�uw6��,ȍU��;���`��p��]7��T�K2^�a�`l.X�y>X,�Q�'��U�rƘ�	R~9`Ȉ������!HSn���N �H`�z-Y\�_,X��;�ߟ�������ht��ǡ���)�!*W�ʾ�!��H���
~+��;��P�)2�`���
WJ�]}BY�t7҆�.X:�����^�,�$�n�Y<%���>R��#����ȾQU�ѣz���Ժ����Lxf��d���I��mm,�j쯪�|`��I����	���:�d���8g����v*�V�[���N\0RrV<�	��5wO���ӆ*H�FNԊE�, ۾����ʆ_.J�j7}�>����&�<de�5��9j��U#Gu�@^�#zC��$��`L]U�T,P�D��!h�N���ђ�o5�U�
��~�wAޑ�N���7�9Y�7[��A>|���F���l�'=\�Fݼ6�٠t*��%3�F����jf�Fs���M��'
kA]�6�<�X]*�5ɇu"b c�-C��b�Ƣ~�7h�2�4q�eų��e�U��[����ʺ��z��,�r%�(�dۣ�c��:��FW��A��D+'XL"4=	Y,%k_:~r������SPec��-�5��W��k���練���U�Xx�Q�	x�
:�y�R���-�Z�SJNc=��ڝQ1|���d�g�SsY�y��~�4	L�i5Y��Ҝ�N�A���[Ww��z��\��2��h�f0E�������1m{�3��S��! �J-��ר^!k#;L�.K�֞dڴ:�BP��r�1���d���%�<��1�՝��[w �00���2C���S�c޾�o���� �kXL�Zm�O��^ᔹE�z�>�7�����"���Mu�_�1������??����&��룳F��@��w� O���B��n�-��li�5f�Cu��͜���FH�n�z+TI��� m�6��:T�n�C@V�|UA#�� 3��`��JW������rqu���F �.m�Զ
-����`4!@��S3�R�7Sژ��j�����d��Eg���G��>ޯwx��Z���Ju��#��ߓ+��sccP�����C����.<���]~d|-��I�.��<��=Q��1�[��@\ +�w��	_J�ҷ��~��&3<�e����T�A#���h~�����LS*�~��cK#� �ط9An�y�O|A���@��A���@�}�pa�:`�;��%��f__(���}���vx��@x'\�����}��ڒ�v�R�_��ao��t�?����To?vn7^�����pPM�m�:�����s4-d�JD�:(Њîĵ���RS}Z���+J�zR��6��8}����^cz����2��[�os�2KJ���/��˄�(䕖����������� ;3R$2���^�ì�W���K�f�*�mnϷ��� h� R��j���)���U���6���������N��&~{μo��6�h֗s&�"^Qv�%������T�Y�i����|f�I�ѡ��f�sPW��Ə1|l��$e��!�4�T��:n���?(���k�����v�<9�6�#�9a���g^D#�]�m�8��"�j?=?yz��>X�8��
2�3������g۰''�9	��"���0X8`�	w#0���ۏ�e3G����, )\Z�@x�P��P>�̇��K�,4���,�CK���@_�@��۝�1��I'�mtm�#���L�	:
�}��K��2<���+_;����)�M�)����r4�bq�]"t�&��1����)h�3=� ���[�>�c��eF�dk&�~�q�������1y�������93Px�Ӈ.9Q�wxΖ�d����s�?&p���j�;��l��������]3�,���X'G�㧧���˝���Z64 ˀ�*������>C�T�YD�d\r��@J��搓�l�9f[��_ӺAv���!�c_�W Ý��w���!+=�<;�-G�_��l�8J�Q�V�jYǊ����@�����u�2���!ܧ���-+3g#�"t�a|:�1��s��ȱQ��O<H0}�{(Xx?A �P�#biﻜ���Q�@P���u����w[XM����E�x�?\\��8X-趞�_��C�ԥP˝�R 6�{�*3�����?��~`�����ϒ鴫f��2�z� b�Խ
m��E/y�_�����9z�w[���+(�&#���c4.�sy���v%��J�P����rF>�9�Z�	I�2rΡ"zzl8��/=�X8褐���Ra��-^�BYЀ��'�Ɉ�v��0��[�����k$_��Q�6l�T�R�sd�xϏ2�C�He�����w�; ���������13��m{0 	��ޕ���M�fj��y��}�g����}	�Ћ2c�3y����q	��fx�Jl�φQ���̝�m߆��0��t焆)���L�¶q� 
m\�3��5�AQ,�R�4N�҅Y0�X�kR/�8@'x��L�Ï�X*<@7K��=G�xO^��W��%[̸�/}��˧H����n�h�a���?3����_,hM��&H���Fwq@ՠ?�#����o�P�-�ul�t�;qc5�s��t�
�]�i$�-�p6vg4�D5e˸z 5��W
�<����~
2�/�8L�]�4?"@�����b�����<��pW��|"bh��rA�a���Q�M���b�5��4�Ŵ��S����^O8.q �������ӳ�l2�V�����<��+�������P����_{%Ff��J�#��.|�=�o(��]�*Ȝ��k�T�:�W�<t�3�bQ�~l���$e�?���[�pnX����o���ݮ�>@:;�y����|���9=�-��ijӇK��_aE���+M8�ܿ�4�k���6�ol���C�{w�`b�A�^x�5��&�n�>;btϔ�90k�
�"�L�2�р����d�Paz*n!�G,]bY\y������м�oP�� @iSX���o]+�
p�En/��g�7���{)l�Z�@���敏�ahDd���q���^����[ ��u��Y|)f+qi�D6�K{N.��d�!�5c�f\�k`�I��qrz��t�ptp�d;��%k�O+]/|��3��8���^��h�\11�5[��f�����_�X�t$i@$�cj�.T��ŕHR��V�s����6��MmjS��Ԧ6��MmjS��Ԧ6��MmjS��t��_��� �  