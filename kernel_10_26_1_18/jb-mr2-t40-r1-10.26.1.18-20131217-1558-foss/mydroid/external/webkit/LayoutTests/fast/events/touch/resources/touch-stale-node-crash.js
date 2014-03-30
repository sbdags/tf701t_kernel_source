document.ontouchstart = touchStartHandler;

function touchStartHandler(e)
{
    var target = e.touches[0].target;
    document.body.removeChild(target);
    window.location = 'resources/send-touch-up.html';
}

description("If this test does not crash then you pass!");

if (window.layoutTestController)
    layoutTestController.waitUntilDone();

runTouchTest(function() {
    eventSender.clearTouchPoints();
    eventSender.addTouchPoint(50, 150);
    eventSender.touchStart();
});
