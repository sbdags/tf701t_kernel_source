description("Tests adding one callback within another");

var e = document.getElementById("e");
var sameFrame;
window.webkitRequestAnimationFrame(function() {
    sameFrame = true;
}, e);
window.webkitRequestAnimationFrame(function() {
    window.webkitRequestAnimationFrame(function() {
        shouldBeFalse("sameFrame");
    }, e);
}, e);
window.webkitRequestAnimationFrame(function() {
    sameFrame = false;
}, e);

// This should fire the three already registered callbacks, but not the one dynamically registered.
if (window.layoutTestController)
    layoutTestController.display();
// This should fire the dynamically registered callback.
if (window.layoutTestController)
    layoutTestController.display();

if (window.layoutTestController)
    layoutTestController.waitUntilDone();

var successfullyParsed = true;

setTimeout(function() {
    isSuccessfullyParsed();
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}, 200);
