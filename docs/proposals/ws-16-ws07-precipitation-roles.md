# WS-07 proposal — precipitation role names for environment weather

**Requested by:** WS-16 environment v2.

Phase 1 works without WS-07 changes by mapping phenomena to existing roles:

| Phenomenon | CreateNiagaraEffect components | Distinct asset |
|---|---|---|
| snow | `rain` + `mist` (ice element, white params) | `NS_EnvSnow` |
| hail | `sparks` (ice element, higher intensity) | `NS_EnvHail` |
| rain | `rain` + `mist` (water element) | `NS_EnvRain` |

**Recommended WS-07 follow-up:** add dedicated `snow`, `hail`, `rain`, `mist` entries to
`UeremcpNiagaraRoles::ResolveEmitterTemplatePath` (RecycleParticlesInView / SimpleSpriteBurst)
so environment does not overload projectile roles. See `ws-16-rain-niagara-create` branch.

Until merged, environment reports `capability_notes` when reusing cross-domain roles.
