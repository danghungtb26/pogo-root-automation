# Security Analysis — Question Template

Use these questions when assessing a security concern, implementing auth/authz, handling sensitive data, or reviewing a potential vulnerability. Security mistakes are often invisible until they're exploited.

---

## Questions

### 1. What is the threat model?
Who are the potential attackers? What are they trying to do?
- **Malicious users:** authenticated users trying to access other users' data
- **External attackers:** unauthenticated access, injection attacks, API abuse
- **Insider threats:** compromised credentials, developer mistakes
- **Automated bots:** scrapers, brute-force attempts

### 2. What data is at risk?
- What sensitive data is involved? (personal info, financial data, credentials, tokens)
- What is the impact if this data is exposed or tampered with?
- Who should have access to it?

### 3. What is the attack surface?
List all the entry points where malicious input could enter the system:
- User inputs (forms, URL params, query strings)
- API endpoints (especially public ones)
- Third-party integrations
- File uploads
- Stored data that is later read and displayed

### 4. What are the existing defenses?
What security controls are already in place?
- Authentication: how is identity verified?
- Authorization: how is access controlled (RLS, role checks, ownership checks)?
- Input validation: where and how is input validated?
- Data sanitization: is output encoded to prevent injection/XSS?

### 5. What are the potential vulnerabilities?
Based on the attack surface and existing defenses, what gaps exist?
Common categories to consider:
- **Broken access control:** can user A access user B's data?
- **Insecure direct object reference:** are IDs guessable and unvalidated?
- **Injection:** SQL, JS, command injection via unsanitized input?
- **Sensitive data exposure:** tokens/secrets in logs, URLs, or error messages?
- **Missing authorization checks:** server-side checks, not just client-side?

### 6. What are the mitigations?
For each vulnerability identified, what is the countermeasure?
- Prefer deny-by-default access control
- Validate on the server, not just the client
- Never trust user-supplied IDs without ownership verification

### 7. Are there compliance or regulatory requirements?
- Is there a data retention policy?
- Are there regional regulations (GDPR, etc.) that affect how data is stored or deleted?
- Are there audit logging requirements?

### 8. How will the security control be tested?
- What tests verify access control is correct?
- What happens if a user sends an unexpected or malformed request?
- Is there a way to test that unauthorized access is actually blocked?
