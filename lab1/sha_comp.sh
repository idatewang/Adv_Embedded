while [[ true ]]; do
	./test3
	dm 0xFFFC0000 | sha256sum > 0000_sha
	dm 0xFFFC2000 | sha256sum > 2000_sha
	echo "Compared..."
	diff 0000_sha 2000_sha
done
