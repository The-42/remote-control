/* This small example adds a button to the example.com page. This button
 * is styled a bit to test if DOM manipulation and styling is working
 * from a JS script.
 */

var btn = document.createElement('button');
btn.textContent = 'I am new here';
btn.style.color = '#EEE';
btn.style.backgroundColor = 'rgba(3,13,168,.5)';
btn.style.borderRadius = '5px';
document.body.appendChild(btn);
