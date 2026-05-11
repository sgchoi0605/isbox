const email = `n_${Date.now()}@ex.com`;
fetch('http://127.0.0.1:8080/api/auth/signup', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({name:'tester', email, password:'12345678', confirmPassword:'12345678', agreeTerms:true})
}).then(async (res) => {
  const text = await res.text();
  console.log(JSON.stringify({status: res.status, text}));
}).catch((e) => { console.error(e); process.exit(1); });
