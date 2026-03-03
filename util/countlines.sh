# Source - https://stackoverflow.com/a/47125807
# Posted by janos, modified by community. See post 'Timeline' for change history
# Retrieved 2026-03-02, License - CC BY-SA 3.0

#!/bin/bash

total=0
find . -type f -name "*.[ch]" | while IFS= read -r file; do
     count=$(grep -c ^ < "$file")
     echo "$file has $count lines"
     ((total += count))
done
echo TOTAL LINES COUNTED:  $total
