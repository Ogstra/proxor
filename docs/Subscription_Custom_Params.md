# Subscription Custom Parameters

Proxor can read extra subscription metadata from HTTP response headers or from
comment lines at the top of the subscription body.

Body parameters use this format:

```text
#parameter-name: value
vless://...
vmess://...
```

If the whole subscription body is base64 encoded, Proxor decodes it before
reading body parameters.

## `fallback-url`

Backup subscription URL. If the main subscription URL is unavailable, returns
HTTP `300`-`599`, or does not respond within 9 seconds, Proxor retries the
update with the saved fallback URL.

Header:

```http
fallback-url: https://example.com/new-subscription-path
```

Body:

```text
#fallback-url: https://example.com/new-subscription-path
```

The fallback URL is stored on the subscription group. It does not replace the
main subscription URL.

## `subscription-ping-onopen-enabled`

Enables automatic URL testing for this subscription's server list when the app
opens.

Accepted true values: `1`, `true`, `yes`.
Accepted false values: `0`, `false`, `no`.

Header:

```http
subscription-ping-onopen-enabled: 1
```

Body:

```text
#subscription-ping-onopen-enabled: 1
```

## `routing`

Routing profile parameter. Proxor uses only the `DirectSites` field and applies
those sites as direct/bypass rules for profiles in that subscription group.

Header with JSON:

```http
routing: {"DirectSites":["example.com","domain:internal.example"]}
```

Body with JSON:

```text
#routing: {"DirectSites":["example.com","domain:internal.example"]}
```

The `routing` value may also be base64/base64url encoded JSON. To clear saved
direct sites, send:

```http
routing: off
```

Supported `DirectSites` items follow the same domain rule syntax as routing
settings:

```text
example.com
domain:example.com
full:api.example.com
keyword:example
regexp:^.*\.example\.com$
geosite:private
```

Proxor does not support deep-link delivery for these parameters. Send parameter
values through HTTP headers or `#parameter: value` body comments.

## Existing Update Parameters

These subscription update parameters are also supported:

```http
profile-update-interval: 24
update-always: 1
profile-title: My Subscription
```

`profile-update-interval` is in hours when received from the subscription
server.
