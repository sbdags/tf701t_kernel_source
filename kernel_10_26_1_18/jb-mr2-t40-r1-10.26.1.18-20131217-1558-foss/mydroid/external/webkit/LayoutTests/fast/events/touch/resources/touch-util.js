function runTouchTest(body, drtMissingMessage) {
    if (window.eventSender) {
        setTimeout(body, 16);
    } else {
        drtMissingMessage = drtMissingMessage || 'This test requires DRT.'
        debug(drtMissingMessage);
    }
}