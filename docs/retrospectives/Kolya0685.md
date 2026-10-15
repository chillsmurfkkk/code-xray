# nlohmann/json: integration notes (B, PR3)

This is a record of the implemented integration and local verification, to be
expanded with the student's personal experience before the Lab 3 demonstration.

The task is importing and saving versioned analysis profiles. nlohmann/json was
already selected in E0 and pinned to 3.12.0; this PR uses it inside the analysis
implementation. Public APIs exchange C++ profiles or text, without exposing JSON.
CMake discovers the installed package; the dependency-free contracts preset can
still disable profile persistence.

Used features: parsing with exceptions, a parser callback for duplicate keys,
object/boolean/string/integer type checks, checked field access, constructing
nested objects and pretty-print serialization. Parsing supplies syntax errors
and UTF-8 handling. Our own code validates schema, required fields, rule names,
threshold ranges, profile labels and safe file replacement.

A key detail is that integer classification includes unsigned numbers. Checking
the unsigned range before conversion avoids wrapping values above INT64_MAX.
Another detail is that the ordinary DOM overwrites duplicate object keys;
rejecting duplicates during parsing prevents ambiguous threshold settings.

The library made nested serialization compact and provided useful structured
exceptions. It does not validate our application schema or save files safely;
those remain our responsibility. Tests include damaged JSON, invalid field types,
unsupported schemas and failed saves, rather than only successful round trips.

Local verification: the profile/rule executable and six existing test executables
passed. Full macOS CMake build remains blocked by an unrelated source-provider
compile error. Windows integration has not been executed here.

Sources used:
- [parse API](https://json.nlohmann.me/api/basic_json/parse/): parser callback and exceptions.
- [is_number_integer](https://json.nlohmann.me/api/basic_json/is_number_integer/): signed/unsigned classification.

For the final retrospective, add personal reasons for choosing this library over
alternatives, installation experience on the target machine, what was confusing,
and what should be changed on the next integration. No personal experience is
claimed in advance by these implementation notes.
