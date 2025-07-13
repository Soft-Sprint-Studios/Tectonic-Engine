sudo apt-get update
sudo apt-get install -y cmake build-essential libsdl2-dev libglew-dev libopenal-dev libbullet-dev libfreetype6-dev binutils curl tar libicu-dev libkrb5-3 libcurl4 libssl-dev

RUNNER_DIR=/mnt/d/githubactions/actions-runner-linux
mkdir -p "$RUNNER_DIR"
cd "$RUNNER_DIR"

if [ ! -f actions-runner-linux-x64-2.326.0.tar.gz ]; then
    curl -O -L https://github.com/actions/runner/releases/download/v2.326.0/actions-runner-linux-x64-2.326.0.tar.gz
fi

tar -xf actions-runner-linux-x64-2.326.0.tar.gz

if [ -f config.sh ]; then
    ./config.sh remove --token "$1" || true
fi

RUNNER_NAME=${3:-linux-runner-1}

pkill -f run.sh || true
pkill -f Runner.Worker || true
sleep 2
./config.sh remove --token "$1" || true

./config.sh --url "$2" --token "$1" --labels linux,wsl --unattended --name "$RUNNER_NAME"

./run.sh
