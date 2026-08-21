#!/bin/bash
# bin/vm-runner.sh — Detect, start, and execute commands on a QEMU VM
#
# This is a prototype. The long-term direction is to port this to Perl
# with Inline::C, then generalize into a multi-host orchestration system.
#
# Vision (not implemented yet):
#   "I need to transition from a Mac host to a Mac guest running in Parallels,
#    which is running an experimental version of WINE. I want this to happen
#    seamlessly and automatically."
#
# The builder would:
#   1. Detect the target host (libvirt, raw QEMU, Parallels, AWS, etc.)
#   2. Start it if needed
#   3. Install any missing dependencies (Parallels tools, CPAN modules)
#   4. Execute the test suite via SSH/WinRM/agent
#   5. Gather logs and report
#
# Usage: ./bin/vm-runner.sh <command> [args...]
#        ./bin/vm-runner.sh --build /path/to/program.c [--output program.exe]
#
# Environment variables:
#   VM_NAME       — VM domain/instance name (default: windows10)
#   VM_IP         — IP address (auto-detected if unset)
#   SSH_USER      — SSH username (default: Administrator)
#   SSH_KEY       — Path to SSH private key (optional)

set -euo pipefail

# ── Configuration ──────────────────────────────────────────────

VM_NAME="${VM_NAME:-windows10}"
VM_IP="${VM_IP:-}"
SSH_USER="${SSH_USER:-Administrator}"
SSH_KEY="${SSH_KEY:-}"

# ── Phase 1: Detect ──────────────────────────────────────────

detect_vm() {
    echo "=== Detecting VM: $VM_NAME ==="

    # Try libvirt first
    if command -v virsh &>/dev/null; then
        local state
        state=$(virsh domstate "$VM_NAME" 2>/dev/null || true)
        if [ -n "$state" ]; then
            echo "  libvirt: $VM_NAME is '$state'"
            VM_STATUS="libvirt"
            return 0
        fi
    fi

    # Try raw QEMU (check for process)
    if pgrep -f "qemu-system-x86_64.*$VM_NAME" &>/dev/null; then
        echo "  qemu: $VM_NAME is running (raw process)"
        VM_STATUS="qemu"
        return 0
    fi

    # Try Parallels (macOS only)
    if command -v prlctl &>/dev/null; then
        local state
        state=$(prlctl list "$VM_NAME" 2>/dev/null | awk '/\|/{print $4}' | head -1)
        if [ -n "$state" ]; then
            echo "  parallels: $VM_NAME is '$state'"
            VM_STATUS="parallels"
            return 0
        fi
    fi

    # Try cloud (generic — would be extended per-provider)
    if command -v aws &>/dev/null 2>&1 && aws ec2 describe-instances --instance-ids "i-$VM_NAME" &>/dev/null 2>&1; then
        echo "  aws: $VM_NAME exists"
        VM_STATUS="aws"
        return 0
    fi

    echo "  ERROR: VM '$VM_NAME' not found"
    return 1
}

# ── Phase 2: Start ──────────────────────────────────────────────

start_vm() {
    echo "=== Starting VM: $VM_NAME ==="

    case "$VM_STATUS" in
        libvirt)
            virsh start "$VM_NAME"
            echo "  Waiting for VM to boot..."
            sleep 30
            return 0
            ;;
        parallels)
            prlctl start "$VM_NAME"
            echo "  Waiting for VM to boot..."
            sleep 30
            return 0
            ;;
        qemu|aws)
            echo "  ERROR: Don't know how to start $VM_STATUS VM '$VM_NAME'"
            return 1
            ;;
        *)
            echo "  ERROR: Unknown VM status '$VM_STATUS'"
            return 1
            ;;
    esac
}

# ── Phase 3: IP Detection ──────────────────────────────────

detect_vm_ip() {
    if [ -n "$VM_IP" ]; then
        echo "  VM IP: $VM_IP (user-provided)"
        return 0
    fi

    echo "  Detecting VM IP..."

    # Try libvirt's domain XML
    if command -v virsh &>/dev/null; then
        local ip
        ip=$(virsh domifaddr "$VM_NAME" 2>/dev/null | awk '/address/{print $2}' | head -1)
        if [ -n "$ip" ]; then
            VM_IP="$ip"
            echo "  VM IP from libvirt: $ip"
            return 0
        fi
    fi

    # Try Parallels
    if command -v prlctl &>/dev/null; then
        local ip
        ip=$(prlctl list "$VM_NAME" -i 2>/dev/null | grep "IP:" | awk '{print $2}' | head -1)
        if [ -n "$ip" ]; then
            VM_IP="$ip"
            echo "  VM IP from Parallels: $ip"
            return 0
        fi
    fi

    # Try ARP table (if VM is on same subnet)
    local ip
    ip=$(arp -a 2>/dev/null | grep -i "$VM_NAME" | awk '{print $2}' | tr -d '()' | head -1)
    if [ -n "$ip" ]; then
        VM_IP="$ip"
        echo "  VM IP from ARP: $ip"
        return 0
    fi

    echo "  ERROR: Could not detect VM IP"
    return 1
}

# ── Phase 4: Execute ──────────────────────────────────────

execute_on_vm() {
    if [ -z "$VM_IP" ]; then
        detect_vm_ip || return 1
    fi

    echo "=== Executing on VM at $VM_IP ==="

    # Method 1: SSH (preferred)
    if [ -n "$SSH_KEY" ] && ssh -o ConnectTimeout=5 -o StrictHostKeyChecking=no -i "$SSH_KEY" "$SSH_USER@$VM_IP" "echo ok" &>/dev/null; then
        echo "  Using SSH with key"
        local cmd="powershell -Command \"$*\""
        ssh -o ConnectTimeout=10 -o StrictHostKeyChecking=no -i "$SSH_KEY" "$SSH_USER@$VM_IP" "$cmd"
        return $?
    fi

    if ssh -o ConnectTimeout=5 -o StrictHostKeyChecking=no "$SSH_USER@$VM_IP" "echo ok" &>/dev/null; then
        echo "  Using SSH (password auth)"
        local cmd="powershell -Command \"$*\""
        ssh -o ConnectTimeout=10 -o StrictHostKeyChecking=no "$SSH_USER@$VM_IP" "$cmd"
        return $?
    fi

    # Method 2: WinRM (PowerShell remoting)
    if command -v python3 &>/dev/null; then
        echo "  Trying WinRM (PowerShell remoting)..."
        python3 -c "
import sys
try:
    import requests
    r = requests.get('http://$VM_IP:5985/wsman', timeout=5)
    print('  WinRM available')
    sys.exit(0)
except:
    sys.exit(1)
" 2>/dev/null && {
            echo "  WinRM detected, using PowerShell remoting..."
            python3 -m winrm "$VM_IP:5985" "$SSH_USER" "powershell -Command \"$(echo "$*" | sed "s/'/''/g")\"" 2>/dev/null || {
                echo "  ERROR: winrm Python module not installed (pip install pywinrm)"
                return 1
            }
            return $?
        }
    fi

    # Method 3: QEMU guest agent (if managed by libvirt)
    if command -v virsh &>/dev/null; then
        echo "  Trying QEMU guest agent..."
        virsh guest-exec "$VM_NAME" "/path/to/exe" 2>/dev/null || {
            echo "  ERROR: guest agent not responding"
            return 1
        }
        return $?
    fi

    # Method 4: QEMU monitor (limited)
    if [ "$VM_STATUS" = "qemu" ]; then
        echo "  QEMU monitor mode — cannot execute commands directly"
        echo "  Tip: Use QEMU's -mon chardev=socket:...,server,nowait and send commands via socat"
        return 1
    fi

    echo "  ERROR: No execution method available"
    echo "  Install SSH server on Windows (winget install OpenSSH.Server) or enable WinRM"
    return 1
}

# ── Main ─────────────────────────────────────────────────────────

detect_vm || { echo "VM not found"; exit 1; }

start_vm || { echo "VM start failed"; exit 1; }

execute_on_vm "$@"
