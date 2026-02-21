# Protobuf (nanopb) for Meshtastic-mini

1. Clone [meshtastic/protobufs](https://github.com/meshtastic/protobufs) and [nanopb](https://github.com/nanopb/nanopb) (or use submodules).
2. Generate C files from selected `.proto` with nanopb options:
   - `mesh.proto` → MeshPacket, Data, User, Position, …
   - `channel.proto`, `config.proto`, `admin.proto` (or deviceonly/localonly as needed)
   - For Serial API you need **ToRadio** and **FromRadio** — they are defined in the same repo (see mesh.proto or an api-like file in protobufs).
3. Protobufs already has `nanopb.proto` and option examples; for embedded limit string and repeated sizes (max_count).
4. Add generated `.c`/`.h` to the build and use for:
   - serializing/deserializing ToRadio/FromRadio on Serial;
   - parsing MeshPacket payload (Data, portnum, payload) after decryption.

See also [nanopb documentation](https://jpa.kapsi.fi/nanopb/documentation/).
