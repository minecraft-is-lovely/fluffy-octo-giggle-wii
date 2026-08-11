#!/usr/bin/env node
/**
 * CubiisOnline Game Server
 * Run: node game-server.js [port]   (default port: 4001)
 *
 * Players connect from Dolphin on the same WiFi network.
 * Tell each player your machine's local IP (e.g. 192.168.1.10).
 */

const net  = require('net');
const PORT = parseInt(process.argv[2] || '4001');
const MAX_ROOM_SIZE = 8;
const NUM_ROOMS     = 6;

let nextId  = 1;
// id → { socket, room(-1=lobby), name, color, x, y, buf }
const clients = new Map();

function roomCount(r) {
    let n = 0;
    for (const c of clients.values()) if (c.room === r) n++;
    return n;
}

function broadcast(room, msg, exceptId) {
    for (const [id, c] of clients)
        if (c.room === room && id !== exceptId)
            try { c.socket.write(msg); } catch {}
}

function cleanup(id) {
    const c = clients.get(id);
    if (!c) return;
    if (c.room >= 0) {
        broadcast(c.room, `LEFT ${id}\n`, id);
        console.log(`  [room ${c.room}] ${c.name} left`);
    }
    clients.delete(id);
    console.log(`[-] client ${id} gone`);
}

function handleLine(id, line) {
    const c = clients.get(id);
    if (!c) return;
    const parts = line.split(' ');
    const cmd   = parts[0];

    if (cmd === 'INFO') {
        const counts = Array.from({length: NUM_ROOMS}, (_, r) => roomCount(r)).join(' ');
        c.socket.write(`ROOMS ${counts}\n`);

    } else if (cmd === 'JOIN' && c.room < 0) {
        const room  = Math.max(0, Math.min(NUM_ROOMS - 1, parseInt(parts[1]) || 0));
        if (roomCount(room) >= MAX_ROOM_SIZE) {
            c.socket.write(`ERROR Room full\n`); return;
        }
        c.name  = (parts[2] || 'PLAYER').substring(0, 12).toUpperCase();
        c.color = Math.max(0, Math.min(8, parseInt(parts[3]) || 0));
        c.x     = 260 + Math.floor(Math.random() * 120);
        c.y     = 420;
        c.room  = room;

        c.socket.write(`WELCOME ${id}\n`);

        // Send existing players to newcomer
        for (const [oid, oc] of clients)
            if (oid !== id && oc.room === room)
                c.socket.write(`JOINED ${oid} ${oc.name} ${oc.color} ${Math.round(oc.x)} ${Math.round(oc.y)}\n`);

        // Announce newcomer to others
        broadcast(room, `JOINED ${id} ${c.name} ${c.color} ${Math.round(c.x)} ${Math.round(c.y)}\n`, id);

        console.log(`  [room ${room}] ${c.name} joined (id=${id})`);

    } else if (cmd === 'POS' && c.room >= 0) {
        c.x = parseFloat(parts[1]) || c.x;
        c.y = parseFloat(parts[2]) || c.y;
        broadcast(c.room, `POS ${id} ${Math.round(c.x)} ${Math.round(c.y)}\n`, id);

    } else if (cmd === 'QUIT') {
        cleanup(id);
        try { c.socket.destroy(); } catch {}
    }
}

const server = net.createServer(socket => {
    const id = nextId++;
    clients.set(id, { socket, room: -1, name: 'PLAYER', color: 0, x: 300, y: 420, buf: '' });
    console.log(`[+] client ${id} connected from ${socket.remoteAddress}`);

    socket.setEncoding('utf8');
    socket.setNoDelay(true);

    socket.on('data', chunk => {
        const c = clients.get(id);
        if (!c) return;
        c.buf += chunk;
        const lines = c.buf.split('\n');
        c.buf = lines.pop();
        for (const l of lines) { const t = l.trim(); if (t) handleLine(id, t); }
    });

    socket.on('close', () => cleanup(id));
    socket.on('error', () => cleanup(id));
});

server.listen(PORT, '0.0.0.0', () => {
    console.log(`=== CubiisOnline Server ===`);
    console.log(`Listening on port ${PORT}`);
    console.log(`Share your local IP with players (e.g. run 'ipconfig' or 'ifconfig')`);
    console.log(`Players enter that IP + port ${PORT} in the Wii app`);
});
