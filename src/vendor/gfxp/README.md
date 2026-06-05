# Vendored gfxp

Upstream: https://github.com/wegfawefgawefg/gfxp

Vendored commit: `fb10177 Improve fixed-point ergonomics`

Update method:

```sh
cp -R /home/vega/Coding/GameDev/gfxp/include/gfxp src/vendor/gfxp/include/
```

Splonks vendors only the small header-only public include tree. Do not make
release builds depend on a sibling `../gfxp` checkout.
