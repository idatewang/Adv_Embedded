while [[ true ]]; do
	./test3
	dm 0xFFFC2000 | grep -o "=.*" | sha256sum > 2000_sha
	dm 0xFFFC0000 | grep -o "=.*" | sha256sum > 0000_sha
	echo "Compared..............."
	DIFF=$(diff 0000_sha 2000_sha)
	if [ "$DIFF" ] 
	then
		diff 0000_sha 2000_sha
	    echo "Not the same."
	else
		diff 0000_sha 2000_sha
		echo "Files are the same."
	fi
done
