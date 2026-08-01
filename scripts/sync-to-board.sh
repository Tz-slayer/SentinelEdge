#!/usr/bin/env bash
# 同步整个项目源码到开发板的脚本
# 使用方式: ./scripts/sync-to-board.sh <开发板IP> [用户名]
# 示例: ./scripts/sync-to-board.sh 192.168.1.100 root

set -e

if [ -z "$1" ]; then
    echo "❌ 错误: 请提供开发板的 IP 地址。"
    echo "💡 用法: $0 <board_ip> [user]"
    echo "💡 示例: $0 192.168.1.100 root"
    exit 1
fi

BOARD_IP=$1
USER=${2:-root}
TARGET_DIR="/root/video_sentinel"
TAR_FILE="/tmp/video_sentinel_sync.tar.gz"

echo "📦 正在打包当前项目源码 (自动排除 build/ 和 .git/ 等无关文件)..."
# 使用 tar 打包，排除掉编译产物和 git 历史记录，以加快传输速度
tar --exclude='./build' \
    --exclude='./build-*' \
    --exclude='./.git' \
    --exclude='./.vscode' \
    --exclude='./.idea' \
    --exclude='./data/*/debug' \
    --exclude='./scripts/.venv' \
    -czf "${TAR_FILE}" .

echo "🚀 正在通过 SCP 协议将源码传输到 ${USER}@${BOARD_IP}:/root/ ..."
scp "${TAR_FILE}" "${USER}@${BOARD_IP}:/root/"

echo "🔧 正在开发板上解压代码到 ${TARGET_DIR} ..."
# 远程执行解压命令，并在解压后清理压缩包
ssh "${USER}@${BOARD_IP}" "mkdir -p ${TARGET_DIR} && tar -xzf /root/video_sentinel_sync.tar.gz -C ${TARGET_DIR} && rm /root/video_sentinel_sync.tar.gz"

# 清理本地临时文件
rm "${TAR_FILE}"

echo "✅ 同步完成！"
echo "接下来你可以登录开发板并进行原生编译："
echo "  ssh ${USER}@${BOARD_IP}"
echo "  cd ${TARGET_DIR}"
echo "  ./scripts/board-native-build.sh"
