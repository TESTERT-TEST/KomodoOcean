#!/usr/bin/env bash

set -euo pipefail

# By default, `komodod` inside the container runs as the `nobody:nogroup` user. All files
# created by the daemon in the data folder will also have these access privileges. To
# bypass the `nobody:nogroup` ownership check on the data folder, you can use the
# `-no-ownership-check` key.

nocheck=0
for arg in "$@"
do
    if [[ "$arg" == "--no-ownership-check" ]] || [[ "$arg" == "-no-ownership-check" ]]; then
        nocheck=1
        break
    fi
done

if [[ "$nocheck" -eq 0 && "$(stat -c '%u:%g' /data)" != "65534:65534" ]]; then
  echo "Folder mounted at /data is not owned by nobody:nogroup, please change its permissions on the host with 'sudo chown -R 65534:65534 path/to/komodo-data'."
  exit 1
fi

/app/fetch-params.sh

# Check and create /data/.komodo directory with current user permissions
if [ ! -d "/data/.komodo" ]; then
    echo "Creating /data/.komodo directory..."
    mkdir -p /data/.komodo
    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to create /data/.komodo directory" >&2
        exit 1
    fi
fi

# Check write permissions to /data/.komodo directory
if ! touch /data/.komodo/.writeable 2>/dev/null; then
    echo "ERROR: Cannot write to /data/.komodo directory. Please check permissions." >&2
    exit 1
fi

# Remove test file after successful check
rm -f /data/.komodo/.writeable

exec /app/komodod -datadir=/data/.komodo "$@"
