-- Dovecot IMAP Append Scan Lua script
--
-- This script is called by the imap_append_scan_lua plugin before a message
-- is saved to a mailbox via the APPEND command.
--
-- It can be used to scan the message for spam or malicious content and
-- reject it before it is stored.

---
-- @function scan_append_message
-- @description Called before a message is appended to a mailbox.
-- @param mail A mail object representing the message being appended.
--             This object has the following methods:
--               - mail:get_username() : returns the username of the user
--                                       appending the message.
--               - mail:get_stream()   : returns a stream object for the
--                                       message content.
-- @return boolean `true` to allow the message to be saved, `false` to reject it.
--                 If the function returns nil or any non-boolean value, the
--                 message will be allowed.
--
function scan_append_message(mail)
  -- Example: Log the username and the first 100 bytes of the message
  --
  -- local username = mail:get_username()
  -- local stream = mail:get_stream()
  -- local data = stream:read(100)
  -- dovecot.log_info("Scanning message for user: " .. username ..
  --                  ", data: " .. data)

  -- For now, we'll just allow everything.
  return true
end
