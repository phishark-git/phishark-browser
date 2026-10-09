# External-provider data processing review

Reviewed 2026-10-09 against official published sources. This records public terms,
not a legal determination or proof of the PhiShark account/runtime configuration.
No account settings, billing, regions or production resources were inspected.

| API surface | Published processing boundary | Required release evidence |
| --- | --- | --- |
| Gemini Developer API | Unpaid-service inputs/outputs may improve Google's products and may be reviewed by people; the terms warn against sensitive data. Paid-service content is not used to improve products, but limited abuse/security/legal logging remains. Regional exceptions are defined in the terms. [Gemini API terms](https://ai.google.dev/gemini-api/terms) | Actual API surface, applicable paid/unpaid status and regional terms; do not send page evidence under an unsuitable free-service policy. Billing status alone is not a zero-retention claim. |
| Vertex AI Gemini API | Google states a training restriction without prior permission. Abuse monitoring and grounding can retain data; Search/Maps grounding retention cannot be disabled while those features are used. Gemini defaults include project-isolated memory caching with a 24-hour TTL. [Google Cloud zero-retention guidance](https://docs.cloud.google.com/gemini-enterprise-agent-platform/resources/zero-data-retention) | Applicable contract, abuse-monitoring scope/exception, caching controls, location and grounding configuration. Keep separate from Developer API. |
| Vertex AI express mode | Express uses the Google Cloud `aiplatform.googleapis.com` surface with API-key authentication and simplified project/location handling. The current overview is marked preview and references Pre-GA terms. [Express overview](https://docs.cloud.google.com/gemini-enterprise-agent-platform/models/start/express-mode/overview) | Exact contract/account tier, API endpoint family and available retention controls. Do not infer Developer API policy from an API key, or standard-project controls from express mode. |
| GroqCloud | Inference content is not retained by default, but reliability/abuse logs may be held for up to 30 days; usage metadata is retained. ZDR controls disable content retention and incompatible stateful features. Retained customer data is stored in the US. [Your data](https://console.groq.com/docs/your-data), [services agreement](https://console.groq.com/docs/legal/services-agreement) | Organization's actual ZDR setting, endpoint/features, account agreement and applicable processing/transfer terms. Prompt-analysis calls are external processing even when PhiShark writes no scan record. |
| Cerebras inference | Its privacy policy states inference inputs/outputs are not retained, while service logs are deleted when no longer needed and other personal data follows purpose/legal retention. [Privacy policy](https://www.cerebras.ai/privacy-policy) | Actual inference endpoint, service contract, regions, operational logging and any intermediary/fallback. Do not expand this statement into a claim about every log or account datum. |

Workspace `SYSTEM_MAP.yaml` and `docs/LLM_INTEGRATIONS.md` identify model callers
and fallback routes. Configured and deployed provider selection remains unknown
until approved evidence establishes it. No provider/model/key migration is made
by the browser changes.

Release disclosure should distinguish three layers: local browser private-mode
storage, PhiShark's transient scan execution with numeric usage accounting, and
external-provider processing. Query parameters can contain personal data even
for private URL-only checks. Record terms-review date, API family, contract and
verified setting names without publishing secret values. Re-review on provider,
model, tier, feature or contract changes.
