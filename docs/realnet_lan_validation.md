# Realnet LAN Validation

This is the two-machine proof path for the Realnet foundation. It verifies that
public browser joins go through `gubsy-roomd` join attempts and still connect to
the game host over a LAN direct candidate.

## Machines

- Host machine: runs `gubsy-roomd` and hosts the Splonks room.
- Client machine: joins through the in-game server browser.

Both machines should be on the same LAN and both repos should be on the same
commit.

Before testing, initialize the verdict file:

```sh
cp docs/realnet_lan_verdict_template.json logs/realnet_lan_verdict.json
```

After the browser and direct checks pass, fill the verdict booleans and audit
it:

```sh
scripts/summarize_realnet_lan_validation.py
```

The full Realnet foundation completion gate is:

```sh
scripts/audit_realnet_foundation.sh
```

That command reruns the focused local/same-machine/LAN-interface checks before
auditing the filled two-machine LAN verdict.

## Automated LAN-Interface Precheck

On the host machine, run:

```sh
scripts/validate_gubsy_roomd_live.sh --lan-interface
```

This starts `gubsy-roomd` on `0.0.0.0`, reaches it through the machine's LAN
IPv4, and runs the headless Splonks public host/browser-join smoke. It is still
a same-machine smoke, so it does not prove firewall or second-machine routing,
but it proves the Realnet browser path works without relying on a localhost
roomd URL.

If the detected LAN address is wrong, override it:

```sh
ROOM_SERVER_HOST=192.168.11.7 scripts/validate_gubsy_roomd_live.sh --lan-interface
```

## Host Machine

Find the host LAN address:

```sh
hostname -I
```

Use the address reachable from the client. Then run:

```sh
GUB_ROOM_SERVER_URL=http://HOST_LAN_IP:8788 \
ROOM_SERVER_BIND=0.0.0.0 \
scripts/run_lobby_human_playtest.sh --host-only
```

In the host window:

1. Open `Host Game`.
2. Select `Host Public`.
3. Confirm the lobby status says it is publicly hosting through
   `gubsy-roomd`.
4. Wait for the client to join.
5. Start the game.

## Client Machine

Run:

```sh
GUB_ROOM_SERVER_URL=http://HOST_LAN_IP:8788 \
scripts/run_lobby_human_playtest.sh --client-only
```

In the client window:

1. Open `Join Game`.
2. Open `Browse Servers`.
3. Select the host room.
4. Confirm the lobby status reports the selected connection path.
5. Wait for the host to start.
6. Enter gameplay when the lobby action becomes `Play`.
7. Confirm client input moves the client player.

## Direct IP Control Check

This verifies the explicit direct path independently of the browser cascade.

Host:

1. Stop the public hosted session.
2. Open `Host Game`.
3. Select `Host Direct`.

Client:

1. Open `Join Game`.
2. Open `Join By IP`.
3. Enter `HOST_LAN_IP` and the host port.
4. Join and confirm gameplay still works.

## Pass Criteria

- The client can fetch the server browser list from the host machine's
  `gubsy-roomd`.
- Browser join creates a Gubsy join attempt before the game joins.
- Browser join selects a LAN/direct candidate rather than treating the room as a
  raw unverified `ip:port` entry.
- Host and client reach the same lobby.
- Host start moves both machines into gameplay.
- No desync or frozen join barrier occurs during the initial join.
- Direct IP LAN join still works as an explicit debug path.

## Evidence To Record

Record the following in `logs/realnet_lan_verdict.json` when validating:

- Host and client commit SHA.
- Host LAN IP and roomd URL.
- Whether browser join passed.
- Selected transport shown by the UI.
- Whether direct IP join passed.
- Any desync replay path if gameplay desyncs.

The Realnet foundation LAN gate is not proven until
`scripts/summarize_realnet_lan_validation.py` reports `ok`.
