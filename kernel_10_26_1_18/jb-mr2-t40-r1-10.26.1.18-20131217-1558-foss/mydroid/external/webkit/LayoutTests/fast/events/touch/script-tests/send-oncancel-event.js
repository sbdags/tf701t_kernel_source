description("Tests that the cancel touch event is sent correctly.");

var touchX = 25;
var touchY = 25;

var cancelEvent = null;

function touchcancelHandler() {
    shouldBeEqualToString("event.type", "touchcancel");
    cancelEvent = event.changedTouches[0];
    shouldBeNonNull("cancelEvent");
    shouldBe("cancelEvent.pageX", touchX.toString());
    shouldBe("cancelEvent.pageY", touchY.toString());
    if (window.layoutTestController) {
        layoutTestController.notifyDone();
        isSuccessfullyParsed(); 
    }
}
    
if (window.layoutTestController)
    window.layoutTestController.waitUntilDone();

window.onload = function() {
    document.addEventListener("touchcancel", touchcancelHandler, false);
    runTouchTest(function() {
        eventSender.addTouchPoint(touchX, touchY);
        eventSender.touchStart();
        eventSender.cancelTouchPoint(0);
        eventSender.touchCancel();
    });
}
