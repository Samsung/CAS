

class MessageException(Exception):
    """
    Base class for Exceptions with error message

    :param message: Exception message
    :type message: str
    """
    def __init__(self, message: str):
        super(MessageException, self).__init__(message)
        self.message = message

class FilterException(MessageException):
    """
    Exception object used by `Filter` class

    :param message: Exception message
    :type message: str
    """

class ParameterException(MessageException):
    """
    Exception object used when parameters have errors

    :param message: Exception message
    :type message: str
    """

class ArgumentException(MessageException):
    """
    Exception object used when arguments have errors

    :param message: Exception message
    :type message: str
    """

class PipelineException(MessageException):
    """
    Exception object used when pipeline data does not match

    :param message: Exception message
    :type message: str
    """

class LibetraceException(MessageException):
    """
    Exception object used when libetrace exception occurs

    :param message: Exception message
    :type message: str
    """

class LibFtdbException(MessageException):
    """
    Exception object used when libftdb exception occurs

    :param message: Exception message
    :type message: str
    """

class EndpointException(MessageException):
    """
    Exception object used when user points to not existing url endpoint

    :param message: Exception message
    :type message: str
    """

class DatabaseException(MessageException):
    """
    Exception object used when database error occurs

    :param message: Exception message
    :type message: str
    """

class IDEGenerationException(MessageException):
    """
    Exception object used when IDE project generation error occurs

    :param message: Exception message
    :type message: str
    """