#!/bin/bash
bash ./.submission-checker-t1.sh . &> build/check
if [ "$(sed -E -n 's/^Grade for .* -([0-9])/\1/p' build/check)" -eq 1 ]; then
	SUBNAME=$(basename "$(pwd)");
		cd ..;
		rm -fr "$SUBNAME.tar.gz";
		tar zcf "$SUBNAME.tar.gz" "$SUBNAME";
else
	echo "HÁ ERROS NO PROGRAMA. Não irei criar pacote!";
	echo "Submeti 3 pedidos: SPAGHETTI, CARNE, SOPA. O programa produziu essa saída com erros: ";
	echo "----------------";
	grep -v 'Grade for ' build/check;
	echo "----------------";
    exit 1
fi
exit 0

