# Source - https://stackoverflow.com/a/47125807
# Posted by janos, modified by community. See post 'Timeline' for change history
# Retrieved 2026-03-02, License - CC BY-SA 3.0

#!/bin/bash

total=0
while IFS= read -r file; do
     count=$(grep -c ^ < "$file")
     echo "$file has $count lines"
     total=$((total + count))
done < <(find . -type f -name "*.[ch]")
echo TOTAL LINES COUNTED:  $total
